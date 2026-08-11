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
  virtual void closeSession(CommandOutput& output) = 0;
};

class CommandCore {
 public:
  static constexpr size_t kMaxCommandLength = 96;

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
    if (length > wifiConfigPrefixLength && strncmp(command, wifiConfigPrefix, wifiConfigPrefixLength) == 0) {
      return services.wifiConfig(command + wifiConfigPrefixLength, output) ? CommandStatus::Ok : CommandStatus::Invalid;
    }
    if (equals(command, length, "config-list")) { services.configList(output); return CommandStatus::Ok; }
    if (startsWith(command, length, "config-get ")) { services.configGet(command + 11, output); return CommandStatus::Ok; }
    if (startsWith(command, length, "config-set ")) {
      return services.configSet(command + 11, output) ? CommandStatus::Ok : CommandStatus::Invalid;
    }
    if (startsWith(command, length, "config-clear ")) {
      return services.configClear(command + 13, output) ? CommandStatus::Ok : CommandStatus::Invalid;
    }
    if (startsWith(command, length, "fs-list ")) { services.fsList(command + 8, output); return CommandStatus::Ok; }
    if (startsWith(command, length, "fs-read ")) { services.fsRead(command + 8, output); return CommandStatus::Ok; }
    if (startsWith(command, length, "fs-write ")) {
      return services.fsWrite(command + 9, output) ? CommandStatus::Ok : CommandStatus::Invalid;
    }
    if (startsWith(command, length, "fs-remove ")) {
      return services.fsRemove(command + 10, output) ? CommandStatus::Ok : CommandStatus::Invalid;
    }
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
    output.line("  wifi-config  Configure Wi-Fi in RAM");
    output.line("  config-list   List persisted configuration keys");
    output.line("  config-get    Read a configuration value");
    output.line("  config-set    Set a configuration value");
    output.line("  config-clear  Clear configuration with --confirm");
    output.line("  fs-list       List LittleFS files");
    output.line("  fs-read       Read a LittleFS file");
    output.line("  fs-write      Write a LittleFS file");
    output.line("  fs-remove     Remove a file with --confirm");
    output.line("  exit         Close the current shell session");
    output.line("  quit         Close the current shell session");
  }
};

}  // namespace esp32shell
