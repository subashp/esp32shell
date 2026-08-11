#pragma once

#include <stddef.h>
#include <string.h>

namespace esp32shell {

enum class CommandStatus { Ok, Empty, Unknown, Invalid };

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
    output.line("error: unknown command; try 'help'");
    return CommandStatus::Unknown;
  }

 private:
  static bool equals(const char* value, size_t length, const char* expected) {
    return strlen(expected) == length && strncmp(value, expected, length) == 0;
  }
  static void help(CommandOutput& output) {
    output.line("Commands:");
    output.line("  help         Show this help");
    output.line("  version      Show firmware version");
    output.line("  device-info  Show chip information");
    output.line("  uptime       Show uptime in milliseconds");
    output.line("  heap         Show free heap");
    output.line("  reboot       Restart the device");
  }
};

}  // namespace esp32shell
