#include "ssh_server.h"

#include <cstring>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "nvs_flash.h"

extern "C" {
#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssh/ssh.h>
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
  return std::memcmp(digest, auth->passwordDigest, sizeof(digest)) == 0
             ? WOLFSSH_USERAUTH_SUCCESS
             : WOLFSSH_USERAUTH_INVALID_PASSWORD;
}

void close_session(WOLFSSH* ssh, int fd) {
  if (ssh != nullptr) {
    wolfSSH_shutdown(ssh);
    wolfSSH_free(ssh);
  }
  if (fd >= 0) close(fd);
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
    WOLFSSH* ssh = wolfSSH_new(context);
    if (ssh == nullptr) {
      close(client);
      continue;
    }
    wolfSSH_SetUserAuthCtx(ssh, &g_auth);
    wolfSSH_set_fd(ssh, client);
    const int accepted = wolfSSH_accept(ssh);
    if (accepted != WS_SUCCESS) {
      ESP_LOGW(kTag, "SSH handshake rejected (%d)", accepted);
      close_session(ssh, client);
      continue;
    }
    ESP_LOGI(kTag, "SSH client authenticated; shell forwarding is next stage");
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
