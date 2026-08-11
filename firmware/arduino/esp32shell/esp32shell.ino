// esp32shell Phase 0/1 bring-up sketch.
// Board: ESP32S3 Dev Module, 32MB Flash, OPI PSRAM.

#include "command_core.h"

using esp32shell::CommandCore;
using esp32shell::CommandOutput;
using esp32shell::DeviceServices;

String readLine;
CommandCore commandCore;

class SerialOutput final : public CommandOutput {
 public:
  void line(const char* text) override { Serial.println(text); }
};

class Esp32Services final : public DeviceServices {
 public:
  void deviceInfo(CommandOutput& output) override {
    Serial.printf("chip=%s cores=%d cpu_mhz=%u", ESP.getChipModel(), ESP.getChipCores(), ESP.getCpuFreqMHz());
    output.line("");
    Serial.printf("flash=%u bytes psram=%u bytes", ESP.getFlashChipSize(), ESP.getPsramSize());
    output.line("");
  }
  void uptime(CommandOutput& output) override {
    Serial.printf("%lu ms", millis());
    output.line("");
  }
  void heap(CommandOutput& output) override {
    Serial.printf("free_heap=%u min_free_heap=%u", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    output.line("");
  }
  void reboot(CommandOutput& output) override {
    output.line("rebooting");
    delay(50);
    ESP.restart();
  }
};

SerialOutput serialOutput;
Esp32Services services;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("esp32shell serial bring-up");
  Serial.println("Type 'help' for commands.");
  Serial.print("esp32shell> ");
}

void loop() {
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      commandCore.dispatch(readLine.c_str(), serialOutput, services);
      readLine = "";
      Serial.print("esp32shell> ");
    } else if (readLine.length() < CommandCore::kMaxCommandLength + 1) {
      readLine += c;
    }
  }
}
