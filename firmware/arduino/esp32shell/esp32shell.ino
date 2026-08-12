// esp32shell Phase 0/1 bring-up sketch.
// Board: ESP32S3 Dev Module, 32MB Flash, OPI PSRAM.

#include "command_core.h"
#include "device_toolbox.h"
#include "storage_policy.h"
#include "ssh_transport.h"
#include "security_policy.h"
#include "ota_service.h"
#include "app_supervisor.h"
#include "wifi_service.h"
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <mbedtls/sha256.h>

using esp32shell::CommandCore;
using esp32shell::CommandOutput;
using esp32shell::DeviceServices;
using esp32shell::WifiDriver;
using esp32shell::WifiService;
using esp32shell::WifiState;
using esp32shell::ConfigOutputPolicy;
using esp32shell::FilesystemPolicy;
using esp32shell::BoundedLog;
using esp32shell::SecurityPolicy;
using esp32shell::OtaSlotManager;
using esp32shell::AppSupervisor;
using esp32shell::AppResourceLimits;

String readLine;
bool lineEndingSeen = false;
CommandCore commandCore;

class ArduinoWifiDriver final : public WifiDriver {
 public:
  void begin(const char* ssid, const char* password) override { WiFi.begin(ssid, password); }
  bool connected() const override { return WiFi.status() == WL_CONNECTED; }
  void disconnect() override {
    // Stop the asynchronous station negotiation before the next WiFi.begin().
    // Without this, Arduino-ESP32 can reject the next profile as "sta is
    // connecting, cannot set config".
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_STA);
    delay(100);
  }
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
  Esp32Services() {
    supervisor_.registerApp("diagnostics", {4096, 20000, 0});
    supervisor_.registerApp("led-blink", {3072, 20000, 0});
  }
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
    Serial.printf("wifi=%s configured=%s active_profile=%u ip=%s rssi=%d", state,
                  wifiService.configured() ? "yes" : "no",
                  static_cast<unsigned>(wifiService.activeSlot()), wifiService.ipAddress(), wifiService.rssi());
    output.line("");
  }
  bool wifiConfig(const char* arguments, CommandOutput& output) override {
    if (arguments == nullptr) { output.line("error: usage wifi-config [<0|1> ]<ssid> <password>"); return false; }
    size_t slot = 0;
    const char* profileArguments = arguments;
    if ((arguments[0] == '0' || arguments[0] == '1') && arguments[1] == ' ') {
      slot = static_cast<size_t>(arguments[0] - '0');
      profileArguments = arguments + 2;
    }
    const char* separator = strchr(profileArguments, ' ');
    if (separator == nullptr || separator == profileArguments || separator[1] == '\0') {
      output.line("error: usage wifi-config [<0|1> ]<ssid> <password>");
      return false;
    }
    char ssid[33] = {};
    const size_t ssidLength = static_cast<size_t>(separator - profileArguments);
    if (ssidLength >= sizeof(ssid)) {
      output.line("error: SSID is too long");
      return false;
    }
    memcpy(ssid, profileArguments, ssidLength);
    ssid[ssidLength] = '\0';
    if (!wifiService.configure(slot, ssid, separator + 1, millis())) {
      output.line("error: invalid Wi-Fi configuration");
      return false;
    }
    char ssidKey[18] = {}, passwordKey[22] = {};
    snprintf(ssidKey, sizeof(ssidKey), "wifi_ssid_%u", static_cast<unsigned>(slot));
    snprintf(passwordKey, sizeof(passwordKey), "wifi_password_%u", static_cast<unsigned>(slot));
    preferences_.putString(ssidKey, ssid);
    preferences_.putString(passwordKey, separator + 1);
    if (slot == 0) {
      preferences_.putString("wifi_ssid", ssid);
      preferences_.putString("wifi_password", separator + 1);
    }
    output.line("wifi profile accepted and persisted");
    return true;
  }
  void wifiProfiles(CommandOutput& output) override {
    for (size_t slot = 0; slot < WifiService::kMaxProfiles; ++slot) {
      char line[64] = {};
      snprintf(line, sizeof(line), "wifi_profile_%u=%s ssid=%s", static_cast<unsigned>(slot),
               wifiService.configured(slot) ? "set" : "unset",
               wifiService.configured(slot) ? wifiService.ssid(slot) : "<unset>");
      output.line(line);
    }
  }
  bool wifiProfileClear(const char* arguments, CommandOutput& output) override {
    if (arguments == nullptr || (arguments[0] != '0' && arguments[0] != '1') ||
        strcmp(arguments + 1, " --confirm") != 0) {
      output.line("error: usage wifi-profile-clear <0|1> --confirm");
      return false;
    }
    const size_t slot = static_cast<size_t>(arguments[0] - '0');
    wifiService.clear(slot);
    char ssidKey[18] = {}, passwordKey[22] = {};
    snprintf(ssidKey, sizeof(ssidKey), "wifi_ssid_%u", static_cast<unsigned>(slot));
    snprintf(passwordKey, sizeof(passwordKey), "wifi_password_%u", static_cast<unsigned>(slot));
    preferences_.remove(ssidKey);
    preferences_.remove(passwordKey);
    if (slot == 0) { preferences_.remove("wifi_ssid"); preferences_.remove("wifi_password"); }
    output.line("wifi profile cleared");
    return true;
  }
  void configList(CommandOutput& output) override {
    output.line("config keys:");
    output.line(preferences_.isKey("wifi_ssid") ? "wifi_ssid=set" : "wifi_ssid=unset");
    output.line(preferences_.isKey("wifi_password") ? "wifi_password=<redacted>" : "wifi_password=unset");
    output.line(preferences_.isKey("wifi_ssid_0") ? "wifi_ssid_0=set" : "wifi_ssid_0=unset");
    output.line(preferences_.isKey("wifi_password_0") ? "wifi_password_0=<redacted>" : "wifi_password_0=unset");
    output.line(preferences_.isKey("wifi_ssid_1") ? "wifi_ssid_1=set" : "wifi_ssid_1=unset");
    output.line(preferences_.isKey("wifi_password_1") ? "wifi_password_1=<redacted>" : "wifi_password_1=unset");
    output.line(preferences_.isKey("ssh_username") ? "ssh_username=set" : "ssh_username=unset");
    output.line(preferences_.isKey("ssh_pw_hash") ? "ssh_password=<protected>" : "ssh_password=unset");
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
    if (strcmp(key, "ssh_password") == 0) {
      if (!SecurityPolicy::passwordIsAcceptable(separator + 1)) {
        output.line("error: password needs 12+ chars with upper, lower, and digit"); return false;
      }
      uint8_t digest[SecurityPolicy::kPasswordDigestSize] = {};
      mbedtls_sha256(reinterpret_cast<const unsigned char*>(separator + 1), strlen(separator + 1), digest, 0);
      preferences_.putBytes("ssh_pw_hash", digest, sizeof(digest));
      preferences_.remove("ssh_password");
    } else if (strcmp(key, "ssh_host_key") == 0) {
      // NVS blob values are bounded below 2 KiB. A 2048-bit RSA DER private
      // key fits this limit and its hex representation fits the 4096-byte line.
      constexpr size_t kMaxHostKeyBytes = 1984;
      uint8_t keyBytes[kMaxHostKeyBytes] = {};
      size_t keyLength = 0;
      if (!decodeHex(separator + 1, keyBytes, sizeof(keyBytes), keyLength) || keyLength < 128) {
        output.line("error: host key must be a bounded ASN.1 DER blob in hexadecimal data"); return false;
      }
      preferences_.putBytes("ssh_host_key", keyBytes, keyLength);
    } else {
      preferences_.putString(key, separator + 1);
    }
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
  void otaStatus(CommandOutput& output) override { output.line(ota_.state()); }
  void appList(CommandOutput& output) override {
    output.line(diagnosticsTask_ == nullptr ? "diagnostics stopped" : "diagnostics running");
    output.line(ledBlinkTask_ == nullptr ? "led-blink stopped" : "led-blink running");
  }
  bool appRun(const char* arguments, CommandOutput& output) override {
    if (arguments == nullptr || (strcmp(arguments, "diagnostics") != 0 && strcmp(arguments, "led-blink") != 0)) {
      output.line("error: unknown app; available apps: diagnostics led-blink"); return false;
    }
    if (strcmp(arguments, "led-blink") == 0) {
      if (ledBlinkTask_ != nullptr) { output.line("led-blink already running"); return true; }
      if (!supervisor_.canStart("led-blink", ESP.getFreeHeap())) { output.line("error: led-blink resource limit exceeded"); return false; }
      pinMode(38, OUTPUT);
      supervisor_.markStarting("led-blink");
      const BaseType_t result = xTaskCreatePinnedToCore(ledBlinkThunk, "led-blink", 3072, this, 1, &ledBlinkTask_, 1);
      if (result != pdPASS) { supervisor_.markFailed("led-blink"); output.line("error: led-blink task could not start"); return false; }
      supervisor_.markRunning("led-blink");
      output.line("led-blink started");
      return true;
    }
    if (diagnosticsTask_ != nullptr) { output.line("diagnostics already running"); return true; }
    if (!supervisor_.canStart("diagnostics", ESP.getFreeHeap())) {
      output.line("error: diagnostics resource limit exceeded"); return false;
    }
    supervisor_.markStarting("diagnostics");
    const BaseType_t result = xTaskCreatePinnedToCore(diagnosticsThunk, "diagnostics", 4096, this, 1,
                                                       &diagnosticsTask_, 1);
    if (result != pdPASS) { supervisor_.markFailed("diagnostics"); output.line("error: diagnostics task could not start"); return false; }
    supervisor_.markRunning("diagnostics");
    logs_.append("diagnostics app started");
    output.line("diagnostics started");
    return true;
  }
  bool appStop(const char* arguments, CommandOutput& output) override {
    if (arguments == nullptr || (strcmp(arguments, "diagnostics") != 0 && strcmp(arguments, "led-blink") != 0)) {
      output.line("error: usage app-stop <diagnostics|led-blink>"); return false;
    }
    if (strcmp(arguments, "led-blink") == 0) {
      if (ledBlinkTask_ == nullptr) { output.line("led-blink already stopped"); return true; }
      vTaskDelete(ledBlinkTask_);
      ledBlinkTask_ = nullptr;
      digitalWrite(38, LOW);
      supervisor_.markStopped("led-blink");
      logs_.append("led-blink app stopped");
      output.line("led-blink stopped");
      return true;
    }
    if (diagnosticsTask_ == nullptr) { output.line("diagnostics already stopped"); return true; }
    vTaskDelete(diagnosticsTask_);
    diagnosticsTask_ = nullptr;
    supervisor_.markStopped("diagnostics");
    logs_.append("diagnostics app stopped");
    output.line("diagnostics stopped");
    return true;
  }
  void appStatus(CommandOutput& output) override {
    output.line(supervisor_.state("diagnostics") == esp32shell::AppLifecycle::Running ? "diagnostics=running" : "diagnostics=stopped");
    output.line(supervisor_.state("led-blink") == esp32shell::AppLifecycle::Running ? "led-blink=running" : "led-blink=stopped");
  }
  void closeSession(CommandOutput& output) override {
    output.line("serial monitor remains active; press Ctrl-C to exit");
  }
  void beginStorage() {
    preferences_.begin("esp32shell", false);
    // The partition table labels this data partition "littlefs". The
    // Arduino default is "spiffs", which would make initialization fail
    // even though the partition exists.
    LittleFS.begin(true, "/littlefs", 10, "littlefs");
  }
  void loadWifi() {
    for (size_t slot = 0; slot < WifiService::kMaxProfiles; ++slot) {
      char ssidKey[18] = {}, passwordKey[22] = {};
      snprintf(ssidKey, sizeof(ssidKey), "wifi_ssid_%u", static_cast<unsigned>(slot));
      snprintf(passwordKey, sizeof(passwordKey), "wifi_password_%u", static_cast<unsigned>(slot));
      String ssid = preferences_.getString(ssidKey, "");
      String password = preferences_.getString(passwordKey, "");
      if (ssid.length() == 0 && slot == 0) ssid = preferences_.getString("wifi_ssid", "");
      if (password.length() == 0 && slot == 0) password = preferences_.getString("wifi_password", "");
      if (ssid.length() > 0 && password.length() > 0) wifiService.configure(slot, ssid.c_str(), password.c_str(), millis());
    }
  }
  void recordLog(const char* message) { logs_.append(message); }
  bool verifySshPassword(const char* password) {
    if (password == nullptr || !preferences_.isKey("ssh_pw_hash")) return false;
    uint8_t expected[SecurityPolicy::kPasswordDigestSize] = {};
    if (preferences_.getBytes("ssh_pw_hash", expected, sizeof(expected)) != sizeof(expected)) return false;
    uint8_t actual[SecurityPolicy::kPasswordDigestSize] = {};
    mbedtls_sha256(reinterpret_cast<const unsigned char*>(password), strlen(password), actual, 0);
    uint8_t difference = 0;
    for (size_t index = 0; index < sizeof(actual); ++index) difference |= actual[index] ^ expected[index];
    return difference == 0;
  }

 private:
  static void diagnosticsThunk(void* context) {
    static_cast<Esp32Services*>(context)->diagnosticsLoop();
  }
  static void ledBlinkThunk(void* context) {
    (void)context;
    for (;;) {
      digitalWrite(38, !digitalRead(38));
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }
  void diagnosticsLoop() {
    for (;;) {
      Serial.printf("diagnostics tick uptime=%lu free_heap=%lu",
                    static_cast<unsigned long>(millis()),
                    static_cast<unsigned long>(ESP.getFreeHeap()));
      Serial.println();
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
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
  static bool decodeHex(const char* text, uint8_t* output, size_t capacity, size_t& length) {
    length = 0;
    const size_t textLength = strlen(text);
    if (textLength == 0 || (textLength % 2) != 0 || textLength / 2 > capacity) return false;
    for (size_t index = 0; index < textLength; index += 2) {
      const auto nibble = [](char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
      };
      const int high = nibble(text[index]);
      const int low = nibble(text[index + 1]);
      if (high < 0 || low < 0) return false;
      output[length++] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
  }
  Preferences preferences_;
  BoundedLog logs_;
  AppSupervisor supervisor_;
  OtaSlotManager ota_;
  TaskHandle_t diagnosticsTask_ = nullptr;
  TaskHandle_t ledBlinkTask_ = nullptr;
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
        Serial.println();
        commandCore.dispatch(readLine.c_str(), serialOutput, services);
        readLine = "";
        Serial.print("esp32shell> ");
      }
      lineEndingSeen = true;
    } else if (readLine.length() < CommandCore::kMaxCommandLength + 1) {
      readLine += c;
      Serial.write(static_cast<uint8_t>(c));
      lineEndingSeen = false;
    }
  }
}
