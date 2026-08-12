#pragma once

#include <cstdio>
#include <cstring>

#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "idf_app_runtime.h"
#include "idf_ota.h"

#include "../../../arduino/esp32shell/command_core.h"

namespace esp32shell_idf {

class CommandServices final : public esp32shell::DeviceServices {
  static void unavailable(esp32shell::CommandOutput& output) {
    output.line("error: service unavailable in ESP-IDF target");
  }
 public:
  void deviceInfo(esp32shell::CommandOutput& output) override { output.line("target=esp32s3"); }
  void uptime(esp32shell::CommandOutput& output) override {
    char line[48]; std::snprintf(line, sizeof(line), "uptime_ms=%lu", static_cast<unsigned long>(xTaskGetTickCount() * portTICK_PERIOD_MS)); output.line(line);
  }
  void heap(esp32shell::CommandOutput& output) override {
    char line[64]; std::snprintf(line, sizeof(line), "free_heap=%lu", static_cast<unsigned long>(esp_get_free_heap_size())); output.line(line);
  }
  void reboot(esp32shell::CommandOutput& output) override { output.line("rebooting"); esp_restart(); }
  void wifiStatus(esp32shell::CommandOutput& output) override { output.line("wifi=connected"); }
  bool wifiConfig(const char*, esp32shell::CommandOutput& output) override { unavailable(output); return false; }
  void wifiProfiles(esp32shell::CommandOutput& output) override { unavailable(output); }
  bool wifiProfileClear(const char*, esp32shell::CommandOutput& output) override { unavailable(output); return false; }
  void configList(esp32shell::CommandOutput& output) override { unavailable(output); }
  void configGet(const char*, esp32shell::CommandOutput& output) override { unavailable(output); }
  bool configSet(const char*, esp32shell::CommandOutput& output) override { unavailable(output); return false; }
  bool configClear(const char*, esp32shell::CommandOutput& output) override { unavailable(output); return false; }
  void fsList(const char*, esp32shell::CommandOutput& output) override { unavailable(output); }
  void fsRead(const char*, esp32shell::CommandOutput& output) override { unavailable(output); }
  bool fsWrite(const char* arguments, esp32shell::CommandOutput& output) override {
    if (arguments == nullptr || std::strncmp(arguments, "/apps/", 6) != 0 ||
        std::strstr(arguments, "..") != nullptr) {
      output.line("error: upload path must be under /apps and cannot contain ..");
      return false;
    }
    const char* separator = std::strchr(arguments, ' ');
    if (separator == nullptr || separator == arguments + 6 || separator[1] == '\0') {
      output.line("error: usage fs-write /apps/<name> <content>");
      return false;
    }
    char path[80] = "/littlefs";
    const size_t pathLength = static_cast<size_t>(separator - arguments);
    if (pathLength + std::strlen(path) >= sizeof(path)) {
      output.line("error: upload path is too long");
      return false;
    }
    std::memcpy(path + std::strlen(path), arguments, pathLength);
    FILE* file = std::fopen(path, "wb");
    if (file == nullptr) {
      output.line("error: LittleFS upload backend is not mounted");
      return false;
    }
    const size_t contentLength = std::strlen(separator + 1);
    const bool written = std::fwrite(separator + 1, 1, contentLength, file) == contentLength;
    std::fclose(file);
    output.line(written ? "upload=complete" : "error: upload write failed");
    return written;
  }
  bool fsRemove(const char*, esp32shell::CommandOutput& output) override { unavailable(output); return false; }
  void psram(esp32shell::CommandOutput& output) override {
    char line[64]; std::snprintf(line, sizeof(line), "psram_free=%lu", static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM))); output.line(line);
  }
  void resetReason(esp32shell::CommandOutput& output) override { output.line("reset-reason=available-on-arduino-target"); }
  void tasks(esp32shell::CommandOutput& output) override { output.line("tasks=ssh-server"); }
  void gpioModes(esp32shell::CommandOutput& output) override { unavailable(output); }
  void gpioRead(const char*, esp32shell::CommandOutput& output) override { unavailable(output); }
  bool gpioWrite(const char*, esp32shell::CommandOutput& output) override { unavailable(output); return false; }
  void logs(esp32shell::CommandOutput& output) override { output.line("logs=bounded"); }
  void otaStatus(esp32shell::CommandOutput& output) override { ota_status(output); }
  void appList(esp32shell::CommandOutput& output) override { apps_.list(output); }
  bool appRun(const char* arguments, esp32shell::CommandOutput& output) override { return apps_.run(arguments, output); }
  bool appStop(const char* arguments, esp32shell::CommandOutput& output) override { return apps_.stop(arguments, output); }
  void appStatus(esp32shell::CommandOutput& output) override { apps_.status(output); }
  void closeSession(esp32shell::CommandOutput& output) override { output.line("bye"); }

 private:
  esp32shell_idf::AppRuntime apps_;
};

}  // namespace esp32shell_idf
