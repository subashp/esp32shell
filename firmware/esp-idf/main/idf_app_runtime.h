#pragma once

#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "../../../arduino/esp32shell/command_core.h"

namespace esp32shell_idf {

class AppRuntime {
 public:
  AppRuntime();
  void list(esp32shell::CommandOutput& output);
  bool run(const char* name, esp32shell::CommandOutput& output);
  bool stop(const char* name, esp32shell::CommandOutput& output);
  void status(esp32shell::CommandOutput& output);

 private:
  enum class State { Stopped, Running, Failed };
  struct AppRecord {
    char name[24];
    bool builtin;
    State state;
    TaskHandle_t task;
    int pin;
    int periodMs;
    char failure[48];
  };

  static constexpr size_t kMaxApps = 4;
  static constexpr size_t kStackBytes = 3072;
  static void taskThunk(void* context);
  bool start(AppRecord* app, esp32shell::CommandOutput& output);
  AppRecord* find(const char* name);
  AppRecord* findOrCreateSigned(const char* name);
  bool loadAndVerify(const char* name, uint8_t** payload, size_t* payloadLength,
                     esp32shell::CommandOutput& output);
  static const char* stateName(State state);

  AppRecord apps_[kMaxApps]{};
  size_t appCount_ = 0;
};

}  // namespace esp32shell_idf
