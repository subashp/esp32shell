#include "idf_app_runtime.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "driver/gpio.h"
#include "led_strip.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
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
led_strip_handle_t g_rgbLed = nullptr;

bool ensure_rgb_led() {
  if (g_rgbLed != nullptr) return true;
  led_strip_config_t stripConfig = {};
  stripConfig.strip_gpio_num = 38;
  stripConfig.max_leds = 1;
  stripConfig.led_model = LED_MODEL_WS2812;
  stripConfig.led_pixel_format = LED_PIXEL_FORMAT_GRB;
  led_strip_rmt_config_t rmtConfig = {};
  rmtConfig.clk_src = RMT_CLK_SRC_DEFAULT;
  rmtConfig.resolution_hz = 10 * 1000 * 1000;
  rmtConfig.mem_block_symbols = 64;
  return led_strip_new_rmt_device(&stripConfig, &rmtConfig, &g_rgbLed) == ESP_OK;
}

void set_rgb(bool on) {
  if (!ensure_rgb_led()) {
    ESP_LOGW(kTag, "RGB LED driver initialization failed");
    return;
  }
  if (on) led_strip_set_pixel(g_rgbLed, 0, 0, 24, 0);
  else led_strip_clear(g_rgbLed);
  led_strip_refresh(g_rgbLed);
}

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
  for (const char* p = name; *p != '\0'; ++p)
    if (!(std::isalnum(static_cast<unsigned char>(*p)) || *p == '-' || *p == '_')) return false;
  return true;
}

class NullOutput final : public esp32shell::CommandOutput {
 public:
  void line(const char*) override {}
};
}

namespace esp32shell_idf {

AppRuntime::AppRuntime() {
  std::strcpy(apps_[0].name, "diagnostics"); apps_[0].builtin = true; apps_[0].state = State::Stopped; apps_[0].pin = -1; apps_[0].periodMs = 1000;
  std::strcpy(apps_[1].name, "led-blink"); apps_[1].builtin = true; apps_[1].state = State::Stopped; apps_[1].pin = 38; apps_[1].periodMs = 500;
  appCount_ = 2;
  xTaskCreate(supervisorThunk, "app-supervisor", 3072, this, 2, &supervisorTask_);
}

const char* AppRuntime::stateName(State state) {
  switch (state) {
    case State::Running: return "running";
    case State::Failed: return "failed";
    default: return "stopped";
  }
}

AppRuntime::AppRecord* AppRuntime::find(const char* name) {
  if (name == nullptr) return nullptr;
  for (size_t i = 0; i < appCount_; ++i)
    if (std::strcmp(apps_[i].name, name) == 0) return &apps_[i];
  return nullptr;
}

AppRuntime::AppRecord* AppRuntime::findOrCreateSigned(const char* name) {
  AppRecord* existing = find(name);
  if (existing != nullptr) return existing;
  if (!safe_name(name) || appCount_ >= kMaxApps) return nullptr;
  AppRecord* app = &apps_[appCount_++];
  std::strncpy(app->name, name, sizeof(app->name) - 1); app->name[sizeof(app->name) - 1] = '\0';
  app->builtin = false; app->state = State::Stopped; app->task = nullptr; app->pin = 38;
  app->periodMs = 500; app->failure[0] = '\0'; app->heartbeat = 0; app->stackWatermark = 0; app->restartCount = 0;
  return app;
}

void AppRuntime::list(esp32shell::CommandOutput& output) {
  for (size_t i = 0; i < appCount_; ++i) {
    char line[112];
    std::snprintf(line, sizeof(line), "app=%s type=%s state=%s", apps_[i].name,
                  apps_[i].builtin ? "builtin" : "signed", stateName(apps_[i].state));
    output.line(line);
  }
  output.line("apps=signed-bundles-require-signature");
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
  const bool valid = verifier.load() && esp32shell::SignedAppBundle::verify(
      header, name, bytes, header.payloadLength, digest, verifier);
  if (!valid) { heap_caps_free(bytes); output.line("error: app bundle signature rejected"); return false; }
  *payload = bytes; *payloadLength = header.payloadLength; return true;
}

bool AppRuntime::start(AppRecord* app, esp32shell::CommandOutput& output) {
  if (app == nullptr) { output.line("error: app is unavailable"); return false; }
  if (app->task != nullptr) { output.line("error: app is already running"); return false; }
  if (esp_get_free_heap_size() < kMinimumFreeHeap) {
    std::strncpy(app->failure, "insufficient heap", sizeof(app->failure) - 1);
    app->state = State::Failed; output.line("error: app resource limit reached"); return false;
  }
  if (xTaskCreate(taskThunk, app->name, kStackBytes, app, 1, &app->task) != pdPASS) {
    app->state = State::Failed; std::strncpy(app->failure, "task allocation failed", sizeof(app->failure) - 1);
    output.line("error: app task could not start"); return false;
  }
  app->state = State::Running; app->failure[0] = '\0'; app->heartbeat = xTaskGetTickCount();
  char line[64]; std::snprintf(line, sizeof(line), "app-started=%s", app->name); output.line(line); return true;
}

bool AppRuntime::run(const char* name, esp32shell::CommandOutput& output) {
  AppRecord* app = find(name);
  if (app == nullptr) {
    uint8_t* payload = nullptr; size_t length = 0;
    if (!loadAndVerify(name, &payload, &length, output)) return false;
    const bool supported = length < 128 && std::strncmp(reinterpret_cast<const char*>(payload), "builtin=led-blink", 18) == 0;
    heap_caps_free(payload);
    if (!supported) { output.line("error: signed app descriptor is unsupported"); return false; }
    app = findOrCreateSigned(name);
  }
  return start(app, output);
}

bool AppRuntime::stop(const char* name, esp32shell::CommandOutput& output) {
  AppRecord* app = find(name);
  if (app == nullptr || app->task == nullptr) { output.line("error: app is not running"); return false; }
  vTaskDelete(app->task); app->task = nullptr; app->state = State::Stopped;
  if (app->pin >= 0) set_rgb(false);
  char line[64]; std::snprintf(line, sizeof(line), "app-stopped=%s", app->name); output.line(line); return true;
}

void AppRuntime::status(esp32shell::CommandOutput& output) {
  for (size_t i = 0; i < appCount_; ++i) {
    char line[112];
    if (apps_[i].state == State::Failed)
      std::snprintf(line, sizeof(line), "app=%s state=failed reason=%s restarts=%u", apps_[i].name, apps_[i].failure, apps_[i].restartCount);
    else
      std::snprintf(line, sizeof(line), "app=%s state=%s stack_free=%u restarts=%u", apps_[i].name, stateName(apps_[i].state), static_cast<unsigned>(apps_[i].stackWatermark), apps_[i].restartCount);
    output.line(line);
  }
}

void AppRuntime::taskThunk(void* context) {
  AppRecord* app = static_cast<AppRecord*>(context);
  // The supervisor is process-lifetime storage; this task intentionally does
  // not reference an SSH session or CommandServices instance.
  for (;;) {
    app->heartbeat = xTaskGetTickCount();
    app->stackWatermark = uxTaskGetStackHighWaterMark(nullptr);
    if (app->stackWatermark < kMinimumStackWords) {
      std::strncpy(app->failure, "stack resource limit", sizeof(app->failure) - 1);
      app->state = State::Failed;
      app->task = nullptr;
      vTaskDelete(nullptr);
      return;
    }
    if (app->pin >= 0) {
      static bool ledOn = false;
      ledOn = !ledOn;
      set_rgb(ledOn);
    } else {
      ESP_LOGI(kTag, "diagnostics app heartbeat heap=%lu",
               static_cast<unsigned long>(esp_get_free_heap_size()));
    }
    vTaskDelay(pdMS_TO_TICKS(app->periodMs));
  }
}

void AppRuntime::supervisorThunk(void* context) {
  auto* runtime = static_cast<AppRuntime*>(context);
  for (;;) {
    runtime->supervise();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void AppRuntime::supervise() {
  const TickType_t now = xTaskGetTickCount();
  for (size_t i = 0; i < appCount_; ++i) {
    AppRecord& app = apps_[i];
    if (app.state != State::Running || app.task == nullptr) continue;
    const TickType_t timeout = pdMS_TO_TICKS(static_cast<uint32_t>(app.periodMs) * 4U);
    if (now - app.heartbeat <= timeout) continue;
    vTaskDelete(app.task);
    app.task = nullptr;
    if (app.restartCount >= kMaximumRestarts) {
      app.state = State::Failed;
      std::strncpy(app.failure, "heartbeat timeout", sizeof(app.failure) - 1);
      continue;
    }
    ++app.restartCount;
    NullOutput ignored;
    start(&app, ignored);
    ESP_LOGW(kTag, "restarted stalled app %s (%u/%u)", app.name, app.restartCount, kMaximumRestarts);
  }
}

}  // namespace esp32shell_idf
