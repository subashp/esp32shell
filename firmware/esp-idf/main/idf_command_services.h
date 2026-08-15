#pragma once

#include <cstdio>
#include <cstring>
#include <dirent.h>

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_chip_info.h"
#include "esp_netif.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_flash.h"

#include "idf_app_runtime.h"
#include "idf_log_buffer.h"
#include "idf_ota.h"
#include "wifi_station.h"
#include "../../../arduino/esp32shell/command_core.h"

namespace esp32shell_idf {

class CommandServices final : public esp32shell::DeviceServices {
  static constexpr char kNvsNamespace[] = "esp32shell";
  static constexpr int kAllowedPins[] = {38};

  static bool readString(nvs_handle_t handle, const char* key, char* value,
                         size_t capacity) {
    size_t length = capacity;
    return nvs_get_str(handle, key, value, &length) == ESP_OK && value[0] != '\0';
  }

  static bool parseSlot(const char* value, unsigned* slot) {
    if (value == nullptr || value[0] < '0' || value[0] > '1' || value[1] != '\0') return false;
    *slot = static_cast<unsigned>(value[0] - '0');
    return true;
  }

  static bool allowedPin(int pin) {
    for (int candidate : kAllowedPins) if (candidate == pin) return true;
    return false;
  }

  static bool parsePin(const char* value, int* pin) {
    if (value == nullptr || value[0] == '\0') return false;
    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    if (*end != '\0' || parsed < 0 || parsed > 48) return false;
    *pin = static_cast<int>(parsed);
    return allowedPin(*pin);
  }

  static bool openNvs(nvs_open_mode_t mode, nvs_handle_t* handle,
                      esp32shell::CommandOutput& output) {
    const esp_err_t result = nvs_open(kNvsNamespace, mode, handle);
    if (result != ESP_OK) {
      output.line("error: NVS unavailable");
      return false;
    }
    return true;
  }

 public:
  void deviceInfo(esp32shell::CommandOutput& output) override {
    esp_chip_info_t chip{};
    esp_chip_info(&chip);
    char line[128];
    std::snprintf(line, sizeof(line), "chip=%s cores=%d revision=%d", CONFIG_IDF_TARGET, chip.cores, chip.revision);
    output.line(line);
    uint32_t flashSize = 0;
    esp_flash_get_size(esp_flash_default_chip, &flashSize);
    std::snprintf(line, sizeof(line), "flash_bytes=%lu psram_bytes=%lu idf=%s",
                  static_cast<unsigned long>(flashSize),
                  static_cast<unsigned long>(esp_psram_get_size()), esp_get_idf_version());
    output.line(line);
  }
  void uptime(esp32shell::CommandOutput& output) override {
    char line[48];
    std::snprintf(line, sizeof(line), "uptime_ms=%lu",
                  static_cast<unsigned long>(xTaskGetTickCount() * portTICK_PERIOD_MS));
    output.line(line);
  }
  void heap(esp32shell::CommandOutput& output) override {
    char line[64];
    std::snprintf(line, sizeof(line), "free_heap=%lu min_free_heap=%lu psram_free=%lu",
                  static_cast<unsigned long>(esp_get_free_heap_size()),
                  static_cast<unsigned long>(esp_get_minimum_free_heap_size()),
                  static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    output.line(line);
  }
  void reboot(esp32shell::CommandOutput& output) override { output.line("rebooting"); esp_restart(); }

  void wifiStatus(esp32shell::CommandOutput& output) override {
    Esp32shellWifiStatus status{};
    esp32shell_wifi_status(&status);
    const char* state = status.rssi == 0 ? (status.connecting ? "connecting" : "offline") : "connected";
    char line[128];
    std::snprintf(line, sizeof(line), "wifi=%s configured=%s active_profile=%u ssid=%s ip=%s rssi=%d",
                  state, status.configured ? "yes" : "no", status.activeSlot,
                  status.activeSsid[0] == '\0' ? "<unset>" : status.activeSsid,
                  status.ipAddress, status.rssi);
    output.line(line);
  }

  bool wifiConfig(const char* arguments, esp32shell::CommandOutput& output) override {
    if (arguments == nullptr) return false;
    unsigned slot = 0;
    const char* cursor = arguments;
    if (arguments[0] >= '0' && arguments[0] <= '1' && arguments[1] == ' ') {
      slot = static_cast<unsigned>(arguments[0] - '0'); cursor = arguments + 2;
    }
    const char* separator = std::strchr(cursor, ' ');
    if (separator == nullptr || separator == cursor || separator[1] == '\0') {
      output.line("error: usage wifi-config [<0|1> ]<ssid> <password>"); return false;
    }
    char ssid[33] = {}; char password[65] = {};
    const size_t ssidLength = static_cast<size_t>(separator - cursor);
    if (ssidLength >= sizeof(ssid) || std::strlen(separator + 1) >= sizeof(password)) {
      output.line("error: Wi-Fi credential is too long"); return false;
    }
    std::memcpy(ssid, cursor, ssidLength); std::strcpy(password, separator + 1);
    nvs_handle_t handle = 0;
    if (!openNvs(NVS_READWRITE, &handle, output)) return false;
    char ssidKey[20]; char passwordKey[24];
    std::snprintf(ssidKey, sizeof(ssidKey), "wifi_ssid_%u", slot);
    std::snprintf(passwordKey, sizeof(passwordKey), "wifi_password_%u", slot);
    bool ok = nvs_set_str(handle, ssidKey, ssid) == ESP_OK &&
              nvs_set_str(handle, passwordKey, password) == ESP_OK;
    if (slot == 0) ok = ok && nvs_set_str(handle, "wifi_ssid", ssid) == ESP_OK &&
                              nvs_set_str(handle, "wifi_password", password) == ESP_OK;
    ok = ok && nvs_commit(handle) == ESP_OK; nvs_close(handle);
    output.line(ok ? "wifi profile accepted and persisted" : "error: Wi-Fi profile save failed");
    return ok;
  }

  void wifiProfiles(esp32shell::CommandOutput& output) override {
    nvs_handle_t handle = 0; if (!openNvs(NVS_READONLY, &handle, output)) return;
    for (unsigned slot = 0; slot < 2; ++slot) {
      char key[20]; char ssid[33] = {};
      std::snprintf(key, sizeof(key), "wifi_ssid_%u", slot);
      if (readString(handle, key, ssid, sizeof(ssid))) {
        char line[80]; std::snprintf(line, sizeof(line), "wifi_profile_%u=set ssid=%s", slot, ssid); output.line(line);
      } else { char line[48]; std::snprintf(line, sizeof(line), "wifi_profile_%u=unset", slot); output.line(line); }
    }
    nvs_close(handle);
  }

  bool wifiProfileClear(const char* arguments, esp32shell::CommandOutput& output) override {
    unsigned ignored = 0;
    if (arguments == nullptr || std::strlen(arguments) < 10 ||
      std::strcmp(arguments + 2, "--confirm") != 0 || arguments[1] != ' ' ||
      !parseSlot(arguments, &ignored)) {
      output.line("error: usage wifi-profile-clear <0|1> --confirm"); return false;
    }
    const unsigned slot = static_cast<unsigned>(arguments[0] - '0');
    nvs_handle_t handle = 0; if (!openNvs(NVS_READWRITE, &handle, output)) return false;
    char key[24]; std::snprintf(key, sizeof(key), "wifi_ssid_%u", slot); nvs_erase_key(handle, key);
    std::snprintf(key, sizeof(key), "wifi_password_%u", slot); nvs_erase_key(handle, key);
    const bool ok = nvs_commit(handle) == ESP_OK; nvs_close(handle);
    output.line(ok ? "wifi profile cleared" : "error: Wi-Fi profile clear failed"); return ok;
  }

  void configList(esp32shell::CommandOutput& output) override {
    nvs_handle_t handle = 0; if (!openNvs(NVS_READONLY, &handle, output)) return;
    output.line("wifi_ssid_0=protected"); output.line("wifi_password_0=<redacted>");
    output.line("wifi_ssid_1=protected"); output.line("wifi_password_1=<redacted>");
    output.line("ssh_username=protected"); output.line("ssh_password=<protected>");
    output.line("ssh_host_key=<protected>"); nvs_close(handle);
  }
  void configGet(const char* arguments, esp32shell::CommandOutput& output) override {
    if (arguments == nullptr || arguments[0] == '\0') { output.line("error: usage config-get <key>"); return; }
    nvs_handle_t handle = 0; if (!openNvs(NVS_READONLY, &handle, output)) return;
    char value[128] = {}; const bool secret = std::strstr(arguments, "password") != nullptr || std::strstr(arguments, "host_key") != nullptr;
    size_t valueLength = sizeof(value);
    if (secret) output.line("value=<protected>");
    else if (nvs_get_str(handle, arguments, value, &valueLength) == ESP_OK) {
      output.line(value);
    } else {
      output.line("error: key not found");
    }
    nvs_close(handle);
  }
  bool configSet(const char* arguments, esp32shell::CommandOutput& output) override {
    const char* separator = arguments == nullptr ? nullptr : std::strchr(arguments, ' ');
    if (separator == nullptr || separator == arguments || separator[1] == '\0') { output.line("error: usage config-set <key> <value>"); return false; }
    char key[32] = {}; const size_t keyLength = static_cast<size_t>(separator - arguments);
    if (keyLength >= sizeof(key)) { output.line("error: key is too long"); return false; }
    std::memcpy(key, arguments, keyLength); const bool secret = std::strstr(key, "password") != nullptr || std::strcmp(key, "ssh_host_key") == 0;
    nvs_handle_t handle = 0; if (!openNvs(NVS_READWRITE, &handle, output)) return false;
    const bool ok = nvs_set_str(handle, key, separator + 1) == ESP_OK && nvs_commit(handle) == ESP_OK; nvs_close(handle);
    output.line(ok ? (secret ? "configuration updated (redacted)" : "configuration updated") : "error: configuration save failed"); return ok;
  }
  bool configClear(const char* arguments, esp32shell::CommandOutput& output) override {
    if (arguments == nullptr || std::strcmp(arguments, "--confirm") != 0) { output.line("error: config-clear requires --confirm"); return false; }
    nvs_handle_t handle = 0; if (!openNvs(NVS_READWRITE, &handle, output)) return false;
    const bool ok = nvs_erase_all(handle) == ESP_OK && nvs_commit(handle) == ESP_OK; nvs_close(handle);
    output.line(ok ? "configuration cleared" : "error: configuration clear failed"); return ok;
  }

  void fsList(const char* arguments, esp32shell::CommandOutput& output) override {
    char path[96] = "/littlefs";
    if (arguments != nullptr && arguments[0] != '\0') {
      if (arguments[0] != '/' || std::strstr(arguments, "..") != nullptr) {
        output.line("error: usage fs-list <directory>"); return;
      }
      std::snprintf(path, sizeof(path), "/littlefs%s", arguments);
    }
    DIR* directory = opendir(path);
    if (directory == nullptr) { output.line("error: LittleFS directory unavailable"); return; }
    char line[96]; struct dirent* entry = nullptr; size_t count = 0;
    while ((entry = readdir(directory)) != nullptr && count < 64) {
      std::strncpy(line, entry->d_name, sizeof(line) - 2);
      line[sizeof(line) - 2] = '\0';
      if (entry->d_type == DT_DIR && std::strlen(line) < sizeof(line) - 1)
        std::strcat(line, "/");
      output.line(line); ++count;
    }
    closedir(directory);
    if (count == 0) output.line("fs=empty");
  }
  void fsRead(const char* arguments, esp32shell::CommandOutput& output) override { if (arguments == nullptr || arguments[0] != '/') { output.line("error: usage fs-read <path>"); return; } char path[96]; std::snprintf(path, sizeof(path), "/littlefs%s", arguments); FILE* file = std::fopen(path, "rb"); if (!file) { output.line("error: file not found"); return; } char buffer[97]; size_t count = std::fread(buffer, 1, sizeof(buffer) - 1, file); buffer[count] = '\0'; std::fclose(file); output.line(buffer); }
  bool fsWrite(const char* arguments, esp32shell::CommandOutput& output) override { if (arguments == nullptr || std::strncmp(arguments, "/apps/", 6) != 0 || std::strstr(arguments, "..") != nullptr) { output.line("error: upload path must be under /apps and cannot contain .."); return false; } const char* separator = std::strchr(arguments, ' '); if (!separator || separator == arguments + 6 || !separator[1]) { output.line("error: usage fs-write /apps/<name> <content>"); return false; } char path[96]; const size_t n = static_cast<size_t>(separator - arguments); if (n + 9 >= sizeof(path)) { output.line("error: upload path is too long"); return false; } std::memcpy(path, "/littlefs", 9); std::memcpy(path + 9, arguments, n); path[n + 9] = '\0'; FILE* file = std::fopen(path, "wb"); if (!file) { output.line("error: LittleFS is not mounted"); return false; } const bool ok = std::fwrite(separator + 1, 1, std::strlen(separator + 1), file) == std::strlen(separator + 1); std::fclose(file); output.line(ok ? "upload=complete" : "error: upload write failed"); return ok; }
  bool fsRemove(const char* arguments, esp32shell::CommandOutput& output) override { if (arguments == nullptr || std::strncmp(arguments, "/apps/", 6) != 0 || std::strstr(arguments, "..") != nullptr || std::strstr(arguments, " --confirm") == nullptr) { output.line("error: usage fs-remove /apps/<path> --confirm"); return false; } char path[96]; std::snprintf(path, sizeof(path), "/littlefs%s", arguments); char* confirm = std::strstr(path, " --confirm"); *confirm = '\0'; const bool ok = std::remove(path) == 0; output.line(ok ? "file removed" : "error: file remove failed"); return ok; }

  void psram(esp32shell::CommandOutput& output) override { char line[64]; std::snprintf(line, sizeof(line), "psram_free=%lu", static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM))); output.line(line); }
  void resetReason(esp32shell::CommandOutput& output) override { char line[64]; std::snprintf(line, sizeof(line), "reset-reason=%d", static_cast<int>(esp_reset_reason())); output.line(line); }
  void tasks(esp32shell::CommandOutput& output) override {
    TaskStatus_t statuses[24] = {};
    uint32_t totalRuntime = 0;
    const UBaseType_t count = uxTaskGetSystemState(statuses, 24, &totalRuntime);
    char line[96];
    std::snprintf(line, sizeof(line), "tasks=%u", static_cast<unsigned>(count)); output.line(line);
    for (UBaseType_t index = 0; index < count; ++index) {
      std::snprintf(line, sizeof(line), "%s stack=%u state=%u",
                    statuses[index].pcTaskName,
                    static_cast<unsigned>(statuses[index].usStackHighWaterMark),
                    static_cast<unsigned>(statuses[index].eCurrentState));
      output.line(line);
    }
  }
  void gpioModes(esp32shell::CommandOutput& output) override { output.line("gpio=allowlisted pins=38"); }
  void gpioRead(const char* arguments, esp32shell::CommandOutput& output) override { int pin = -1; if (!parsePin(arguments, &pin)) { output.line("error: usage gpio-read <allowlisted-pin>"); return; } char line[48]; std::snprintf(line, sizeof(line), "gpio=%d value=%d", pin, gpio_get_level(static_cast<gpio_num_t>(pin))); output.line(line); }
  bool gpioWrite(const char* arguments, esp32shell::CommandOutput& output) override { if (arguments == nullptr) { output.line("error: usage gpio-write <allowlisted-pin> <0|1>"); return false; } const char* separator = std::strchr(arguments, ' '); int pin = -1; char pinText[8] = {}; if (!separator || static_cast<size_t>(separator - arguments) >= sizeof(pinText) || (separator[1] != '0' && separator[1] != '1') || separator[2] != '\0') { output.line("error: usage gpio-write <allowlisted-pin> <0|1>"); return false; } std::memcpy(pinText, arguments, static_cast<size_t>(separator - arguments)); if (!parsePin(pinText, &pin)) { output.line("error: usage gpio-write <allowlisted-pin> <0|1>"); return false; } gpio_set_direction(static_cast<gpio_num_t>(pin), GPIO_MODE_OUTPUT); gpio_set_level(static_cast<gpio_num_t>(pin), separator[1] - '0'); output.line("gpio=updated"); return true; }
  void logs(esp32shell::CommandOutput& output) override { esp32shell_idf::log_dump(output); }
  void dmesg(esp32shell::CommandOutput& output) override { esp32shell_idf::log_dump(output); }
  void otaStatus(esp32shell::CommandOutput& output) override { ota_status(output); }
  void appList(esp32shell::CommandOutput& output) override { apps().list(output); }
  bool appRun(const char* arguments, esp32shell::CommandOutput& output) override { return apps().run(arguments, output); }
  bool appStop(const char* arguments, esp32shell::CommandOutput& output) override { return apps().stop(arguments, output); }
  void appStatus(esp32shell::CommandOutput& output) override { apps().status(output); }
  void closeSession(esp32shell::CommandOutput& output) override { output.line("bye"); }

 private:
  static AppRuntime& apps() { static AppRuntime instance; return instance; }
};

}  // namespace esp32shell_idf
