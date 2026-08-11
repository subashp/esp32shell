#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "../../../arduino/esp32shell/command_core.h"

namespace esp32shell_idf {

class AppRuntime {
 public:
  void list(esp32shell::CommandOutput& output);
  bool run(const char* name, esp32shell::CommandOutput& output);
  bool stop(const char* name, esp32shell::CommandOutput& output);
  void status(esp32shell::CommandOutput& output);

 private:
  static void taskThunk(void* context);
  void taskLoop();
  bool loadAndVerify(const char* name, uint8_t** payload, size_t* payloadLength,
                     esp32shell::CommandOutput& output);

  TaskHandle_t task_ = nullptr;
  char running_[32] = {};
  int pin_ = -1;
  int periodMs_ = 500;
};

}  // namespace esp32shell_idf
