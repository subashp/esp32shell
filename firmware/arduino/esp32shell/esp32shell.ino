// esp32shell Phase 0/1 bring-up sketch.
// Board: ESP32S3 Dev Module, 32MB Flash, OPI PSRAM.

#include "command_core.h"
#include "wifi_service.h"
#include <WiFi.h>

using esp32shell::CommandCore;
using esp32shell::CommandOutput;
using esp32shell::DeviceServices;
using esp32shell::WifiDriver;
using esp32shell::WifiService;
using esp32shell::WifiState;

String readLine;
CommandCore commandCore;

class ArduinoWifiDriver final : public WifiDriver {
 public:
  void begin(const char* ssid, const char* password) override { WiFi.begin(ssid, password); }
  bool connected() const override { return WiFi.status() == WL_CONNECTED; }
  void disconnect() override { WiFi.disconnect(); }
  int rssi() const override { return WiFi.RSSI(); }
  const char* ipAddress() const override {
    static char address[16];
    WiFi.localIP().toString().toCharArray(address, sizeof(address));
    return address;
  }
};

ArduinoWifiDriver wifiDriver;
WifiService wifiService(wifiDriver);

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
  void wifiStatus(CommandOutput& output) override {
    const char* state = wifiService.state() == WifiState::Connected ? "connected" :
                        wifiService.state() == WifiState::Connecting ? "connecting" : "offline";
    Serial.printf("wifi=%s configured=%s ip=%s rssi=%d", state,
                  wifiService.configured() ? "yes" : "no", wifiService.ipAddress(), wifiService.rssi());
    output.line("");
  }
  bool wifiConfig(const char* arguments, CommandOutput& output) override {
    const char* separator = strchr(arguments, ' ');
    if (separator == nullptr || separator == arguments || separator[1] == '\0') {
      output.line("error: usage wifi-config <ssid> <password>");
      return false;
    }
    char ssid[33] = {};
    const size_t ssidLength = static_cast<size_t>(separator - arguments);
    if (ssidLength >= sizeof(ssid)) {
      output.line("error: SSID is too long");
      return false;
    }
    memcpy(ssid, arguments, ssidLength);
    ssid[ssidLength] = '\0';
    if (!wifiService.configure(ssid, separator + 1, millis())) {
      output.line("error: invalid Wi-Fi configuration");
      return false;
    }
    output.line("wifi configuration accepted in RAM");
    return true;
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
  wifiService.tick(millis());
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
