#pragma once

#include <cstdio>
#include <cstring>

#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
  void configList(esp32shell::CommandOutput& output) override { unavailable(output); }
  void configGet(const char*, esp32shell::CommandOutput& output) override { unavailable(output); }
  bool configSet(const char*, esp32shell::CommandOutput& output) override { unavailable(output); return false; }
  bool configClear(const char*, esp32shell::CommandOutput& output) override { unavailable(output); return false; }
  void fsList(const char*, esp32shell::CommandOutput& output) override { unavailable(output); }
  void fsRead(const char*, esp32shell::CommandOutput& output) override { unavailable(output); }
  bool fsWrite(const char*, esp32shell::CommandOutput& output) override { unavailable(output); return false; }
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
  void otaStatus(esp32shell::CommandOutput& output) override { output.line("ota=managed-by-arduino-target"); }
  void appList(esp32shell::CommandOutput& output) override { output.line("apps=0"); }
  bool appRun(const char*, esp32shell::CommandOutput& output) override { unavailable(output); return false; }
  bool appStop(const char*, esp32shell::CommandOutput& output) override { unavailable(output); return false; }
  void appStatus(esp32shell::CommandOutput& output) override { output.line("apps=0"); }
  void closeSession(esp32shell::CommandOutput& output) override { output.line("bye"); }
};

}  // namespace esp32shell_idf
