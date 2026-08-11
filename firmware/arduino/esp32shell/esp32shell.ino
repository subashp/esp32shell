// esp32shell Phase 0/1 bring-up sketch.
// Board: ESP32S3 Dev Module, 32MB Flash, OPI PSRAM.

#include "command_core.h"
#include "device_toolbox.h"
#include "storage_policy.h"
#include "ssh_transport.h"
#include "wifi_service.h"
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>

using esp32shell::CommandCore;
using esp32shell::CommandOutput;
using esp32shell::DeviceServices;
using esp32shell::WifiDriver;
using esp32shell::WifiService;
using esp32shell::WifiState;
using esp32shell::ConfigOutputPolicy;
using esp32shell::FilesystemPolicy;
using esp32shell::BoundedLog;

String readLine;
bool lineEndingSeen = false;
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
    Serial.printf("chip=%s cores=%d cpu_mhz=%lu", ESP.getChipModel(), ESP.getChipCores(),
                  static_cast<unsigned long>(ESP.getCpuFreqMHz()));
    output.line("");
    Serial.printf("flash=%lu bytes psram=%lu bytes",
                  static_cast<unsigned long>(ESP.getFlashChipSize()),
                  static_cast<unsigned long>(ESP.getPsramSize()));
    output.line("");
  }
  void uptime(CommandOutput& output) override {
    Serial.printf("%lu ms", millis());
    output.line("");
  }
  void heap(CommandOutput& output) override {
    Serial.printf("free_heap=%lu min_free_heap=%lu",
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(ESP.getMinFreeHeap()));
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
    preferences_.putString("wifi_ssid", ssid);
    preferences_.putString("wifi_password", separator + 1);
    output.line("wifi configuration accepted and persisted");
    return true;
  }
  void configList(CommandOutput& output) override {
    output.line("config keys:");
    output.line(preferences_.isKey("wifi_ssid") ? "wifi_ssid=set" : "wifi_ssid=unset");
    output.line(preferences_.isKey("wifi_password") ? "wifi_password=<redacted>" : "wifi_password=unset");
    output.line(preferences_.isKey("ssh_username") ? "ssh_username=set" : "ssh_username=unset");
    output.line(preferences_.isKey("ssh_password") ? "ssh_password=<redacted>" : "ssh_password=unset");
    output.line(preferences_.isKey("ssh_host_key") ? "ssh_host_key=<protected>" : "ssh_host_key=unset");
  }
  void configGet(const char* arguments, CommandOutput& output) override {
    if (arguments == nullptr || arguments[0] == '\0') { output.line("error: usage config-get <key>"); return; }
    if (ConfigOutputPolicy::isSecretKey(arguments)) {
      output.line("<redacted>");
      return;
    }
    if (!preferences_.isKey(arguments)) { output.line("error: key is not set"); return; }
    String value = preferences_.getString(arguments, "");
    output.line(value.c_str());
  }
  bool configSet(const char* arguments, CommandOutput& output) override {
    const char* separator = arguments == nullptr ? nullptr : strchr(arguments, ' ');
    if (separator == nullptr || separator == arguments || separator[1] == '\0') {
      output.line("error: usage config-set <key> <value>"); return false;
    }
    char key[32] = {};
    const size_t keyLength = static_cast<size_t>(separator - arguments);
    if (keyLength >= sizeof(key)) { output.line("error: key is too long"); return false; }
    memcpy(key, arguments, keyLength);
    if (!isAllowedConfigKey(key)) { output.line("error: key is not allowed"); return false; }
    preferences_.putString(key, separator + 1);
    output.line(ConfigOutputPolicy::isSecretKey(key) ? "configuration updated (redacted)" : "configuration updated");
    return true;
  }
  bool configClear(const char* arguments, CommandOutput& output) override {
    if (arguments == nullptr || strcmp(arguments, "--confirm") != 0) {
      output.line("error: config-clear requires --confirm"); return false;
    }
    preferences_.clear();
    wifiService.clear();
    output.line("configuration cleared");
    return true;
  }
  void fsList(const char* arguments, CommandOutput& output) override {
    const char* path = arguments == nullptr || arguments[0] == '\0' ? "/" : arguments;
    if (!FilesystemPolicy::validPath(path)) { output.line("error: invalid path"); return; }
    File directory = LittleFS.open(path, FILE_READ);
    if (!directory || !directory.isDirectory()) { output.line("error: directory not found"); return; }
    File entry = directory.openNextFile();
    size_t count = 0;
    while (entry && count++ < 32) {
      Serial.printf("%s %lu bytes", entry.name(), static_cast<unsigned long>(entry.size()));
      output.line("");
      entry = directory.openNextFile();
    }
  }
  void fsRead(const char* arguments, CommandOutput& output) override {
    if (!FilesystemPolicy::validPath(arguments)) { output.line("error: invalid path"); return; }
    File file = LittleFS.open(arguments, FILE_READ);
    if (!file || file.isDirectory()) { output.line("error: file not found"); return; }
    if (file.size() > 512) { output.line("error: file is too large"); return; }
    String value = file.readString();
    output.line(value.c_str());
  }
  bool fsWrite(const char* arguments, CommandOutput& output) override {
    const char* separator = arguments == nullptr ? nullptr : strchr(arguments, ' ');
    if (separator == nullptr || separator == arguments) { output.line("error: usage fs-write <path> <content>"); return false; }
    char path[FilesystemPolicy::kMaxPathLength + 1] = {};
    const size_t pathLength = static_cast<size_t>(separator - arguments);
    if (pathLength > FilesystemPolicy::kMaxPathLength) { output.line("error: path is too long"); return false; }
    memcpy(path, arguments, pathLength);
    if (!FilesystemPolicy::validPath(path) || strlen(separator + 1) > 512) { output.line("error: invalid path or content too large"); return false; }
    File file = LittleFS.open(path, FILE_WRITE);
    if (!file) { output.line("error: cannot open file"); return false; }
    file.print(separator + 1);
    file.close();
    output.line("file written");
    return true;
  }
  bool fsRemove(const char* arguments, CommandOutput& output) override {
    if (arguments == nullptr) { output.line("error: usage fs-remove <path> --confirm"); return false; }
    const char* separator = strstr(arguments, " --confirm");
    if (separator == nullptr) { output.line("error: fs-remove requires --confirm"); return false; }
    char path[FilesystemPolicy::kMaxPathLength + 1] = {};
    const size_t pathLength = static_cast<size_t>(separator - arguments);
    if (pathLength > FilesystemPolicy::kMaxPathLength) { output.line("error: path is too long"); return false; }
    memcpy(path, arguments, pathLength);
    if (!FilesystemPolicy::validPath(path) || !LittleFS.remove(path)) { output.line("error: remove failed"); return false; }
    output.line("file removed");
    return true;
  }
  void psram(CommandOutput& output) override {
    Serial.printf("psram=%s size=%lu free=%lu", ESP.getPsramSize() > 0 ? "yes" : "no",
                  static_cast<unsigned long>(ESP.getPsramSize()), static_cast<unsigned long>(ESP.getFreePsram()));
    output.line("");
  }
  void resetReason(CommandOutput& output) override {
    Serial.printf("reset-reason=%d", static_cast<int>(esp_reset_reason()));
    output.line("");
  }
  void tasks(CommandOutput& output) override {
    TaskStatus_t statuses[12] = {};
    uint32_t totalRuntime = 0;
    const UBaseType_t count = uxTaskGetSystemState(statuses, 12, &totalRuntime);
    Serial.printf("tasks=%lu", static_cast<unsigned long>(count));
    output.line("");
    for (UBaseType_t index = 0; index < count && index < 12; ++index) {
      Serial.printf("%s stack=%u", statuses[index].pcTaskName,
                    static_cast<unsigned int>(statuses[index].usStackHighWaterMark));
      output.line("");
    }
  }
  void gpioModes(CommandOutput& output) override {
    output.line("gpio allowlist: 1,2,4,5,6,7,15,16,17,18,38");
  }
  void gpioRead(const char* arguments, CommandOutput& output) override {
    int pin = -1;
    if (arguments == nullptr || sscanf(arguments, "%d", &pin) != 1 || !gpioAllowed(pin)) {
      output.line("error: GPIO pin is not allowlisted"); return;
    }
    pinMode(pin, INPUT);
    Serial.printf("gpio=%d value=%d", pin, digitalRead(pin));
    output.line("");
  }
  bool gpioWrite(const char* arguments, CommandOutput& output) override {
    int pin = -1;
    int value = -1;
    if (arguments == nullptr || sscanf(arguments, "%d %d", &pin, &value) != 2 || !gpioAllowed(pin) || (value != 0 && value != 1)) {
      output.line("error: usage gpio-write <allowlisted-pin> <0|1>"); return false;
    }
    pinMode(pin, OUTPUT);
    digitalWrite(pin, value);
    output.line("gpio updated");
    return true;
  }
  void logs(CommandOutput& output) override {
    if (logs_.count() == 0) { output.line("logs=empty"); return; }
    for (size_t index = 0; index < logs_.count(); ++index) output.line(logs_.at(index));
  }
  void closeSession(CommandOutput& output) override {
    output.line("serial monitor remains active; press Ctrl-C to exit");
  }
  void beginStorage() {
    preferences_.begin("esp32shell", false);
    LittleFS.begin(true);
  }
  void loadWifi() {
    String ssid = preferences_.getString("wifi_ssid", "");
    String password = preferences_.getString("wifi_password", "");
    if (ssid.length() > 0 && password.length() > 0) wifiService.configure(ssid.c_str(), password.c_str(), millis());
  }
  void recordLog(const char* message) { logs_.append(message); }

 private:
  static bool gpioAllowed(int pin) {
    static const int allowedPins[] = {1, 2, 4, 5, 6, 7, 15, 16, 17, 18, 38};
    for (int allowed : allowedPins) if (allowed == pin) return true;
    return false;
  }
  static bool isAllowedConfigKey(const char* key) {
    return strcmp(key, "wifi_ssid") == 0 || strcmp(key, "wifi_password") == 0 ||
           strcmp(key, "ssh_username") == 0 || strcmp(key, "ssh_password") == 0 ||
           strcmp(key, "ssh_host_key") == 0;
  }
  Preferences preferences_;
  BoundedLog logs_;
};

SerialOutput serialOutput;
Esp32Services services;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("esp32shell serial bring-up");
  services.beginStorage();
  services.loadWifi();
  services.recordLog("boot complete");
  Serial.println("Type 'help' for commands.");
  Serial.print("esp32shell> ");
}

void loop() {
  wifiService.tick(millis());
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      // Serial monitors commonly send CRLF. Treat the pair as one line ending.
      if (!lineEndingSeen) {
        commandCore.dispatch(readLine.c_str(), serialOutput, services);
        readLine = "";
        Serial.print("esp32shell> ");
      }
      lineEndingSeen = true;
    } else if (readLine.length() < CommandCore::kMaxCommandLength + 1) {
      readLine += c;
      lineEndingSeen = false;
    }
  }
}
