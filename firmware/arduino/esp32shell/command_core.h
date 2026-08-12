#pragma once

#include <stddef.h>
#include <string.h>

namespace esp32shell {

enum class CommandStatus { Ok, Empty, Unknown, Invalid, SessionClosed };

class CommandOutput {
 public:
  virtual ~CommandOutput() = default;
  virtual void line(const char* text) = 0;
};

class DeviceServices {
 public:
  virtual ~DeviceServices() = default;
  virtual void deviceInfo(CommandOutput& output) = 0;
  virtual void uptime(CommandOutput& output) = 0;
  virtual void heap(CommandOutput& output) = 0;
  virtual void reboot(CommandOutput& output) = 0;
  virtual void wifiStatus(CommandOutput& output) = 0;
  virtual bool wifiConfig(const char* arguments, CommandOutput& output) = 0;
  virtual void configList(CommandOutput& output) = 0;
  virtual void configGet(const char* arguments, CommandOutput& output) = 0;
  virtual bool configSet(const char* arguments, CommandOutput& output) = 0;
  virtual bool configClear(const char* arguments, CommandOutput& output) = 0;
  virtual void fsList(const char* arguments, CommandOutput& output) = 0;
  virtual void fsRead(const char* arguments, CommandOutput& output) = 0;
  virtual bool fsWrite(const char* arguments, CommandOutput& output) = 0;
  virtual bool fsRemove(const char* arguments, CommandOutput& output) = 0;
  virtual void psram(CommandOutput& output) = 0;
  virtual void resetReason(CommandOutput& output) = 0;
  virtual void tasks(CommandOutput& output) = 0;
  virtual void gpioModes(CommandOutput& output) = 0;
  virtual void gpioRead(const char* arguments, CommandOutput& output) = 0;
  virtual bool gpioWrite(const char* arguments, CommandOutput& output) = 0;
  virtual void logs(CommandOutput& output) = 0;
  virtual void otaStatus(CommandOutput& output) = 0;
  virtual void appList(CommandOutput& output) = 0;
  virtual bool appRun(const char* arguments, CommandOutput& output) = 0;
  virtual bool appStop(const char* arguments, CommandOutput& output) = 0;
  virtual void appStatus(CommandOutput& output) = 0;
  virtual void closeSession(CommandOutput& output) = 0;
};

class CommandCore {
 public:
  // Authentication provisioning may carry a DER host key encoded as hex.
  // Keep the line bounded, but large enough for a 2048-bit RSA key.
  static constexpr size_t kMaxCommandLength = 4096;

  CommandStatus dispatch(const char* input, CommandOutput& output,
                         DeviceServices& services) const {
    if (input == nullptr) {
      output.line("error: invalid command");
      return CommandStatus::Invalid;
    }
    const char* command = input;
    while (*command == ' ' || *command == '\t') ++command;
    size_t length = strlen(command);
    while (length > 0 && (command[length - 1] == ' ' || command[length - 1] == '\t')) --length;
    if (length == 0) return CommandStatus::Empty;
    if (length > kMaxCommandLength) {
      output.line("error: command is too long");
      return CommandStatus::Invalid;
    }
    if (equals(command, length, "help")) { help(output); return CommandStatus::Ok; }
    if (equals(command, length, "version")) { output.line("esp32shell 0.1.0"); return CommandStatus::Ok; }
    if (equals(command, length, "device-info")) { services.deviceInfo(output); return CommandStatus::Ok; }
    if (equals(command, length, "uptime")) { services.uptime(output); return CommandStatus::Ok; }
    if (equals(command, length, "heap")) { services.heap(output); return CommandStatus::Ok; }
    if (equals(command, length, "reboot")) { services.reboot(output); return CommandStatus::Ok; }
    if (equals(command, length, "wifi-status")) { services.wifiStatus(output); return CommandStatus::Ok; }
    if (equals(command, length, "exit") || equals(command, length, "quit")) {
      services.closeSession(output);
      return CommandStatus::SessionClosed;
    }
    const char* wifiConfigPrefix = "wifi-config ";
    const size_t wifiConfigPrefixLength = strlen(wifiConfigPrefix);
    if (equals(command, length, "wifi-config")) {
      output.line("error: usage wifi-config <ssid> <password>");
      return CommandStatus::Invalid;
    }
    if (length > wifiConfigPrefixLength && strncmp(command, wifiConfigPrefix, wifiConfigPrefixLength) == 0) {
      return services.wifiConfig(command + wifiConfigPrefixLength, output) ? CommandStatus::Ok : CommandStatus::Invalid;
    }
    if (equals(command, length, "config-list")) { services.configList(output); return CommandStatus::Ok; }
    if (equals(command, length, "config-get")) { output.line("error: usage config-get <key>"); return CommandStatus::Invalid; }
    if (startsWith(command, length, "config-get ")) { services.configGet(command + 11, output); return CommandStatus::Ok; }
    if (equals(command, length, "config-set")) { output.line("error: usage config-set <key> <value>"); return CommandStatus::Invalid; }
    if (startsWith(command, length, "config-set ")) {
      return services.configSet(command + 11, output) ? CommandStatus::Ok : CommandStatus::Invalid;
    }
    if (equals(command, length, "config-clear")) { output.line("error: usage config-clear --confirm"); return CommandStatus::Invalid; }
    if (startsWith(command, length, "config-clear ")) {
      return services.configClear(command + 13, output) ? CommandStatus::Ok : CommandStatus::Invalid;
    }
    if (equals(command, length, "fs-list")) { output.line("error: usage fs-list <directory>"); return CommandStatus::Invalid; }
    if (startsWith(command, length, "fs-list ")) { services.fsList(command + 8, output); return CommandStatus::Ok; }
    if (equals(command, length, "fs-read")) { output.line("error: usage fs-read <path>"); return CommandStatus::Invalid; }
    if (startsWith(command, length, "fs-read ")) { services.fsRead(command + 8, output); return CommandStatus::Ok; }
    if (equals(command, length, "fs-write")) { output.line("error: usage fs-write <path> <content>"); return CommandStatus::Invalid; }
    if (startsWith(command, length, "fs-write ")) {
      return services.fsWrite(command + 9, output) ? CommandStatus::Ok : CommandStatus::Invalid;
    }
    if (equals(command, length, "fs-remove")) { output.line("error: usage fs-remove <path> --confirm"); return CommandStatus::Invalid; }
    if (startsWith(command, length, "fs-remove ")) {
      return services.fsRemove(command + 10, output) ? CommandStatus::Ok : CommandStatus::Invalid;
    }
    if (equals(command, length, "psram")) { services.psram(output); return CommandStatus::Ok; }
    if (equals(command, length, "reset-reason")) { services.resetReason(output); return CommandStatus::Ok; }
    if (equals(command, length, "tasks")) { services.tasks(output); return CommandStatus::Ok; }
    if (equals(command, length, "gpio-modes")) { services.gpioModes(output); return CommandStatus::Ok; }
    if (equals(command, length, "gpio-read")) { output.line("error: usage gpio-read <allowlisted-pin>"); return CommandStatus::Invalid; }
    if (startsWith(command, length, "gpio-read ")) { services.gpioRead(command + 10, output); return CommandStatus::Ok; }
    if (equals(command, length, "gpio-write")) { output.line("error: usage gpio-write <allowlisted-pin> <0|1>"); return CommandStatus::Invalid; }
    if (startsWith(command, length, "gpio-write ")) {
      return services.gpioWrite(command + 11, output) ? CommandStatus::Ok : CommandStatus::Invalid;
    }
    if (equals(command, length, "logs")) { services.logs(output); return CommandStatus::Ok; }
    if (equals(command, length, "ota-status")) { services.otaStatus(output); return CommandStatus::Ok; }
    if (equals(command, length, "app-list")) { services.appList(output); return CommandStatus::Ok; }
    if (equals(command, length, "app-run")) { output.line("error: usage app-run <name>"); return CommandStatus::Invalid; }
    if (startsWith(command, length, "app-run ")) {
      return services.appRun(command + 8, output) ? CommandStatus::Ok : CommandStatus::Invalid;
    }
    if (equals(command, length, "app-stop")) { output.line("error: usage app-stop <name>"); return CommandStatus::Invalid; }
    if (startsWith(command, length, "app-stop ")) {
      return services.appStop(command + 9, output) ? CommandStatus::Ok : CommandStatus::Invalid;
    }
    if (equals(command, length, "app-status")) { services.appStatus(output); return CommandStatus::Ok; }
    output.line("error: unknown command; try 'help'");
    return CommandStatus::Unknown;
  }

 private:
  static bool equals(const char* value, size_t length, const char* expected) {
    return strlen(expected) == length && strncmp(value, expected, length) == 0;
  }
  static bool startsWith(const char* value, size_t length, const char* prefix) {
    const size_t prefixLength = strlen(prefix);
    return length > prefixLength && strncmp(value, prefix, prefixLength) == 0;
  }
  static void help(CommandOutput& output) {
    output.line("Commands:");
    output.line("  help         Show this help");
    output.line("  version      Show firmware version");
    output.line("  device-info  Show chip information");
    output.line("  uptime       Show uptime in milliseconds");
    output.line("  heap         Show free heap");
    output.line("  reboot       Restart the device");
    output.line("  wifi-status  Show Wi-Fi state");
    output.line("  wifi-config  Configure and persist Wi-Fi");
    output.line("  config-list   List persisted configuration keys");
    output.line("  config-get    Read a configuration value");
    output.line("  config-set    Set a configuration value");
    output.line("  config-clear  Clear configuration with --confirm");
    output.line("  fs-list       List LittleFS files");
    output.line("  fs-read       Read a LittleFS file");
    output.line("  fs-write      Write a LittleFS file");
    output.line("  fs-remove     Remove a file with --confirm");
    output.line("  psram         Show PSRAM usage");
    output.line("  reset-reason  Show the last reset reason");
    output.line("  tasks         Show bounded task diagnostics");
    output.line("  gpio-modes    Show allowlisted GPIO pins");
    output.line("  gpio-read     Read an allowlisted GPIO pin");
    output.line("  gpio-write    Write an allowlisted GPIO pin");
    output.line("  logs          Show bounded device logs");
    output.line("  ota-status    Show OTA slot state");
    output.line("  app-list      List built-in apps");
    output.line("  app-run       Launch a built-in app");
    output.line("  app-stop      Stop a built-in app");
    output.line("  app-status    Show app lifecycle state");
    output.line("  exit         Close the current shell session");
    output.line("  quit         Close the current shell session");
  }
};

}  // namespace esp32shell
