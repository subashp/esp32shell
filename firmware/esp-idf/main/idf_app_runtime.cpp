#include "idf_app_runtime.h"

#include <cstdio>
#include <cctype>
#include <cstring>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "nvs.h"

#include "../../../arduino/esp32shell/signed_app_bundle.h"

extern "C" {
#include <wolfssl/wolfcrypt/ed25519.h>
#include <wolfssl/wolfcrypt/sha256.h>
}

namespace {
constexpr char kTag[] = "esp32shell-app";
constexpr char kNvsNamespace[] = "esp32shell";
constexpr size_t kHeaderSize = sizeof(esp32shell::SignedAppBundleHeader);
constexpr size_t kMaxPayload = esp32shell::SignedAppBundle::kMaxPayloadLength;

class WolfDigest final : public esp32shell::AppBundleDigest {
 public:
  bool sha256(const uint8_t* data, size_t length, uint8_t digest[32]) override {
    wc_Sha256 sha;
    return wc_InitSha256(&sha) == 0 && wc_Sha256Update(&sha, data, length) == 0 &&
           wc_Sha256Final(&sha, digest) == 0;
  }
};

class WolfSignatureVerifier final : public esp32shell::AppBundleSignatureVerifier {
 public:
  bool load() {
    nvs_handle_t nvs = 0;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &nvs) != ESP_OK) return false;
    uint8_t publicKey[32] = {};
    size_t length = sizeof(publicKey);
    const bool loaded = nvs_get_blob(nvs, "app_signing_pub", publicKey, &length) == ESP_OK &&
                        length == sizeof(publicKey);
    nvs_close(nvs);
    if (!loaded || wc_ed25519_init(&key_) != 0 ||
        wc_ed25519_import_public(publicKey, sizeof(publicKey), &key_) != 0) return false;
    ready_ = true;
    return true;
  }
  bool verify(const uint8_t digest[32], const uint8_t signature[64]) override {
    if (!ready_) return false;
    int verified = 0;
    return wc_ed25519_verify_msg(signature, 64, digest, 32, &verified, &key_) == 0 && verified == 1;
  }
 private:
  ed25519_key key_{};
  bool ready_ = false;
};

bool safe_name(const char* name) {
  if (name == nullptr || name[0] == '\0' || std::strlen(name) >= 24) return false;
  for (const char* p = name; *p != '\0'; ++p) {
    if (!(std::isalnum(static_cast<unsigned char>(*p)) || *p == '-' || *p == '_')) return false;
  }
  return true;
}
}

namespace esp32shell_idf {

void AppRuntime::list(esp32shell::CommandOutput& output) {
  output.line("apps=signatures-required");
  if (task_ != nullptr) output.line(running_);
}

bool AppRuntime::loadAndVerify(const char* name, uint8_t** payload, size_t* payloadLength,
                               esp32shell::CommandOutput& output) {
  if (!safe_name(name)) { output.line("error: invalid app name"); return false; }
  char path[64] = {};
  std::snprintf(path, sizeof(path), "/littlefs/apps/%s.bundle", name);
  FILE* file = std::fopen(path, "rb");
  if (file == nullptr) { output.line("error: signed app bundle not found"); return false; }
  esp32shell::SignedAppBundleHeader header{};
  if (std::fread(&header, 1, kHeaderSize, file) != kHeaderSize ||
      header.payloadLength == 0 || header.payloadLength > kMaxPayload) {
    std::fclose(file); output.line("error: invalid app bundle header"); return false;
  }
  uint8_t* bytes = static_cast<uint8_t*>(heap_caps_malloc(header.payloadLength, MALLOC_CAP_8BIT));
  if (bytes == nullptr || std::fread(bytes, 1, header.payloadLength, file) != header.payloadLength) {
    if (bytes != nullptr) heap_caps_free(bytes);
    std::fclose(file); output.line("error: app bundle payload read failed"); return false;
  }
  std::fclose(file);
  WolfDigest digest;
  WolfSignatureVerifier verifier;
  const bool valid = verifier.load() &&
                     esp32shell::SignedAppBundle::verify(header, name, bytes,
                                                         header.payloadLength, digest, verifier);
  if (!valid) { heap_caps_free(bytes); output.line("error: app bundle signature rejected"); return false; }
  *payload = bytes;
  *payloadLength = header.payloadLength;
  return true;
}

bool AppRuntime::run(const char* name, esp32shell::CommandOutput& output) {
  if (task_ != nullptr) { output.line("error: an app is already running"); return false; }
  uint8_t* payload = nullptr;
  size_t length = 0;
  if (!loadAndVerify(name, &payload, &length, output)) return false;
  const char* descriptor = reinterpret_cast<const char*>(payload);
  const bool supported = length < 128 && std::strncmp(descriptor, "builtin=led-blink", 18) == 0;
  if (!supported) { heap_caps_free(payload); output.line("error: signed app descriptor is unsupported"); return false; }
  pin_ = 38;
  periodMs_ = 500;
  task_ = nullptr;
  std::strncpy(running_, name, sizeof(running_) - 1);
  heap_caps_free(payload);
  if (xTaskCreate(taskThunk, "signed-app", 3072, this, 1, &task_) != pdPASS) {
    running_[0] = '\0'; output.line("error: signed app task could not start"); return false;
  }
  output.line("signed app started");
  return true;
}

bool AppRuntime::stop(const char* name, esp32shell::CommandOutput& output) {
  if (task_ == nullptr || std::strcmp(name, running_) != 0) {
    output.line("error: signed app is not running"); return false;
  }
  vTaskDelete(task_);
  task_ = nullptr;
  gpio_set_level(static_cast<gpio_num_t>(pin_), 0);
  running_[0] = '\0';
  output.line("signed app stopped");
  return true;
}

void AppRuntime::status(esp32shell::CommandOutput& output) {
  if (task_ == nullptr) output.line("signed-app=stopped");
  else { output.line("signed-app=running"); output.line(running_); }
}

void AppRuntime::taskThunk(void* context) { static_cast<AppRuntime*>(context)->taskLoop(); }

void AppRuntime::taskLoop() {
  gpio_set_direction(static_cast<gpio_num_t>(pin_), GPIO_MODE_OUTPUT);
  for (;;) {
    gpio_set_level(static_cast<gpio_num_t>(pin_), !gpio_get_level(static_cast<gpio_num_t>(pin_)));
    vTaskDelay(pdMS_TO_TICKS(periodMs_));
  }
}

}  // namespace esp32shell_idf
