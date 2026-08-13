#include "ssh_server.h"

#include <cstring>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "idf_command_services.h"

extern "C" {
#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssh/ssh.h>
#include <wolfssh/wolfsftp.h>
}

namespace {
constexpr char kTag[] = "esp32shell-ssh";
constexpr char kNvsNamespace[] = "esp32shell";
constexpr uint16_t kPort = 22222;
constexpr size_t kMaxUsername = 32;
constexpr size_t kMaxBlob = 8192;

struct AuthConfig {
  char username[kMaxUsername] = {};
  uint8_t passwordDigest[WC_SHA256_DIGEST_SIZE] = {};
  uint8_t hostKey[kMaxBlob] = {};
  size_t hostKeyLength = 0;
};

static AuthConfig g_auth;
static TaskHandle_t g_task = nullptr;
static bool g_startRequested = false;

bool load_blob(nvs_handle_t nvs, const char* key, uint8_t* out, size_t capacity,
               size_t* length) {
  size_t size = capacity;
  const esp_err_t err = nvs_get_blob(nvs, key, out, &size);
  if (err != ESP_OK || size == 0 || size > capacity) return false;
  *length = size;
  return true;
}

bool load_auth_config() {
  nvs_handle_t nvs = 0;
  if (nvs_open(kNvsNamespace, NVS_READONLY, &nvs) != ESP_OK) return false;
  size_t usernameLength = sizeof(g_auth.username);
  const bool usernameOk = nvs_get_str(nvs, "ssh_username", g_auth.username,
                                      &usernameLength) == ESP_OK &&
                          g_auth.username[0] != '\0';
  size_t digestLength = 0;
  const bool digestOk = load_blob(nvs, "ssh_pw_hash", g_auth.passwordDigest,
                                  sizeof(g_auth.passwordDigest), &digestLength) &&
                        digestLength == sizeof(g_auth.passwordDigest);
  const bool hostKeyOk = load_blob(nvs, "ssh_host_key", g_auth.hostKey,
                                   sizeof(g_auth.hostKey), &g_auth.hostKeyLength);
  nvs_close(nvs);
  ESP_LOGI(kTag, "SSH credentials loaded: username_length=%u host_key_bytes=%u",
           static_cast<unsigned>(std::strlen(g_auth.username)),
           static_cast<unsigned>(g_auth.hostKeyLength));
  return usernameOk && digestOk && hostKeyOk;
}

int user_auth(byte authType, WS_UserAuthData* data, void* context) {
  if (context == nullptr || data == nullptr ||
      authType != WOLFSSH_USERAUTH_PASSWORD) {
    return WOLFSSH_USERAUTH_FAILURE;
  }
  const auto* auth = static_cast<const AuthConfig*>(context);
  if (data->usernameSz != std::strlen(auth->username) ||
      std::memcmp(data->username, auth->username, data->usernameSz) != 0) {
    ESP_LOGW(kTag, "SSH username rejected: received_length=%u expected_length=%u",
             static_cast<unsigned>(data->usernameSz),
             static_cast<unsigned>(std::strlen(auth->username)));
    return WOLFSSH_USERAUTH_INVALID_USER;
  }

  uint8_t digest[WC_SHA256_DIGEST_SIZE] = {};
  wc_Sha256 sha;
  if (wc_InitSha256(&sha) != 0 ||
      wc_Sha256Update(&sha, data->sf.password.password,
                      data->sf.password.passwordSz) != 0 ||
      wc_Sha256Final(&sha, digest) != 0) {
    return WOLFSSH_USERAUTH_FAILURE;
  }
  const bool matches = std::memcmp(digest, auth->passwordDigest, sizeof(digest)) == 0;
  ESP_LOGI(kTag, "SSH password authentication %s (length=%u)",
           matches ? "accepted" : "rejected",
           static_cast<unsigned>(data->sf.password.passwordSz));
  return matches ? WOLFSSH_USERAUTH_SUCCESS : WOLFSSH_USERAUTH_INVALID_PASSWORD;
}

void close_session(WOLFSSH* ssh, int fd) {
  if (ssh != nullptr) {
    wolfSSH_shutdown(ssh);
    wolfSSH_free(ssh);
  }
  if (fd >= 0) close(fd);
}

static volatile bool g_shellRequestAccepted = false;

int shell_request(WOLFSSH_CHANNEL* channel, void*) {
  if (channel == nullptr) return WS_BAD_ARGUMENT;
  (void)channel;
  g_shellRequestAccepted = true;
  ESP_LOGI(kTag, "SSH shell channel request accepted");
  return WS_SUCCESS;
}

class SshOutput final : public esp32shell::CommandOutput {
 public:
  explicit SshOutput(WOLFSSH* ssh) : ssh_(ssh) {}
  void line(const char* text) override {
    if (text == nullptr) return;
    char buffer[esp32shell::CommandCore::kMaxCommandLength + 3] = {};
    std::snprintf(buffer, sizeof(buffer), "%s\r\n", text);
    const word32 length = static_cast<word32>(std::strlen(buffer));
    for (int attempt = 0; attempt < 5; ++attempt) {
      const int sent = wolfSSH_stream_send(ssh_, reinterpret_cast<byte*>(buffer), length);
      if (sent == static_cast<int>(length)) return;
      if (sent != WS_WANT_WRITE && sent != WS_WINDOW_FULL) {
        ESP_LOGW(kTag, "SSH shell output failed (%d, attempt=%d)", sent, attempt + 1);
        return;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGW(kTag, "SSH shell output remained back-pressured");
  }
 private:
  WOLFSSH* ssh_;
};

void serve_shell(WOLFSSH* ssh) {
  esp32shell::CommandCore core;
  esp32shell_idf::CommandServices services;
  SshOutput output(ssh);
  // Authentication/channel open completes before interactive requests arrive.
  // Keep servicing wolfSSH until the shell callback has accepted the request;
  // one worker pass is insufficient because Windows sends pty-req and shell
  // as separate channel requests.
  while (!g_shellRequestAccepted) {
    const int channelResult = wolfSSH_worker(ssh, nullptr);
    if (channelResult != WS_SUCCESS && channelResult != WS_CHAN_RXD &&
        channelResult != WS_WANT_READ && channelResult != WS_WANT_WRITE) {
      ESP_LOGW(kTag, "SSH shell channel setup failed (%d)", channelResult);
      return;
    }
  }
  output.line("esp32shell ssh shell");
  output.line("Type 'help' for commands.");
  output.line("esp32shell>");
  char line[esp32shell::CommandCore::kMaxCommandLength + 1] = {};
  size_t used = 0;
  uint8_t input[64] = {};
  while (true) {
    const int received = wolfSSH_stream_read(ssh, input, sizeof(input));
    if (received == WS_WANT_READ || received == WS_WANT_WRITE) continue;
    if (received <= 0) break;
    for (int i = 0; i < received; ++i) {
      const char ch = static_cast<char>(input[i]);
      if (ch == '\r' || ch == '\n') {
        line[used] = '\0';
        if (core.dispatch(line, output, services) == esp32shell::CommandStatus::SessionClosed) return;
        used = 0;
        output.line("esp32shell>");
      } else if (ch == '\b' || ch == 0x7f) {
        if (used > 0) --used;
      } else if (used < esp32shell::CommandCore::kMaxCommandLength) {
        line[used++] = ch;
      } else {
        output.line("error: command is too long");
        used = 0;
      }
    }
  }
}

int serve_sftp(WOLFSSH* ssh) {
  if (wolfSSH_SFTP_SetDefaultPath(ssh, "/littlefs") != WS_SUCCESS) return WS_FATAL_ERROR;
  ESP_LOGI(kTag, "SFTP subsystem accepted with root /littlefs");
  int result = WS_CHAN_RXD;
  const int socketFd = wolfSSH_get_fd(ssh);
  for (;;) {
    if (result == WS_CHAN_RXD || result == WS_WANT_WRITE ||
        wolfSSH_SFTP_PendingSend(ssh)) {
      result = wolfSSH_SFTP_read(ssh);
    } else {
      fd_set readSet;
      FD_ZERO(&readSet);
      FD_SET(socketFd, &readSet);
      timeval timeout{};
      timeout.tv_sec = 1;
      const int selected = select(socketFd + 1, &readSet, nullptr, nullptr, &timeout);
      if (selected < 0) return WS_SOCKET_ERROR_E;
      if (selected == 0) continue;
      result = wolfSSH_worker(ssh, nullptr);
    }
    const int error = wolfSSH_get_error(ssh);
    if (error == WS_EOF || error == WS_CHANNEL_CLOSED || result == WS_EOF ||
        result == WS_CHANNEL_CLOSED) return WS_SUCCESS;
    if (result == WS_SUCCESS || result == WS_CHAN_RXD || result == WS_WANT_READ ||
        result == WS_WANT_WRITE || error == WS_WANT_READ || error == WS_WANT_WRITE ||
        error == WS_CHAN_RXD || error == WS_WINDOW_FULL) {
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    return result;
  }
}

void ssh_server_task(void*) {
  g_task = xTaskGetCurrentTaskHandle();
  if (!load_auth_config()) {
    ESP_LOGW(kTag, "SSH disabled: NVS needs username, password digest, and ASN.1 host key");
    g_task = nullptr;
    vTaskDelete(nullptr);
    return;
  }
  if (wolfSSH_Init() != WS_SUCCESS) {
    ESP_LOGE(kTag, "wolfSSH_Init failed");
    g_task = nullptr;
    vTaskDelete(nullptr);
    return;
  }

  WOLFSSH_CTX* context = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_SERVER, nullptr);
  if (context == nullptr ||
      wolfSSH_CTX_UsePrivateKey_buffer(context, g_auth.hostKey,
                                       static_cast<word32>(g_auth.hostKeyLength),
                                       WOLFSSH_FORMAT_ASN1) != WS_SUCCESS) {
    ESP_LOGE(kTag, "SSH host key unavailable or invalid");
    if (context != nullptr) wolfSSH_CTX_free(context);
    wolfSSH_Cleanup();
    g_task = nullptr;
    vTaskDelete(nullptr);
    return;
  }
  wolfSSH_SetUserAuth(context, user_auth);
  wolfSSH_CTX_SetBanner(context, "esp32shell authenticated shell\r\n");
  wolfSSH_CTX_SetChannelReqShellCb(context, shell_request);

  const int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (listener < 0) {
    ESP_LOGE(kTag, "SSH listener socket failed");
    wolfSSH_CTX_free(context);
    wolfSSH_Cleanup();
    g_task = nullptr;
    vTaskDelete(nullptr);
    return;
  }
  int reuse = 1;
  setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(kPort);
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
      listen(listener, 1) < 0) {
    ESP_LOGE(kTag, "SSH listener bind/listen failed");
    close(listener);
    wolfSSH_CTX_free(context);
    wolfSSH_Cleanup();
    g_task = nullptr;
    vTaskDelete(nullptr);
    return;
  }
  ESP_LOGI(kTag, "authenticated SSH listener started on port %u", kPort);

  while (g_startRequested) {
    const int client = accept(listener, nullptr, nullptr);
    if (client < 0) continue;
    ESP_LOGI(kTag, "SSH TCP client accepted");
    WOLFSSH* ssh = wolfSSH_new(context);
    if (ssh == nullptr) {
      close(client);
      continue;
    }
    wolfSSH_SetUserAuthCtx(ssh, &g_auth);
    wolfSSH_set_fd(ssh, client);
    g_shellRequestAccepted = false;
    const int accepted = wolfSSH_accept(ssh);
    ESP_LOGI(kTag, "SSH handshake result=%d error=%d", accepted,
             wolfSSH_get_error(ssh));
    if (accepted == WS_SFTP_COMPLETE) {
      serve_sftp(ssh);
      close_session(ssh, client);
      continue;
    }
    if (accepted != WS_SUCCESS) {
      ESP_LOGW(kTag, "SSH handshake rejected (%d)", accepted);
      close_session(ssh, client);
      continue;
    }
    ESP_LOGI(kTag, "SSH client authenticated");
    serve_shell(ssh);
    ESP_LOGI(kTag, "SSH shell session ended");
    close_session(ssh, client);
  }
  close(listener);
  wolfSSH_CTX_free(context);
  wolfSSH_Cleanup();
  g_task = nullptr;
  vTaskDelete(nullptr);
}
}  // namespace

extern "C" void esp32shell_ssh_start_after_wifi() {
  if (g_startRequested || g_task != nullptr) return;
  g_startRequested = true;
  xTaskCreate(ssh_server_task, "ssh-server", 8192, nullptr, 3, &g_task);
}
