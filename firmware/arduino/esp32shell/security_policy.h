#pragma once

#include <string.h>

namespace esp32shell {

class SecurityPolicy {
 public:
  static bool passwordIsAcceptable(const char* password) {
    if (password == nullptr || strlen(password) < 12) return false;
    bool upper = false, lower = false, digit = false;
    for (const char* p = password; *p != '\0'; ++p) {
      upper = upper || (*p >= 'A' && *p <= 'Z');
      lower = lower || (*p >= 'a' && *p <= 'z');
      digit = digit || (*p >= '0' && *p <= '9');
    }
    return upper && lower && digit;
  }
  static bool requiresConfirmation(const char* command) {
    return command != nullptr && (strcmp(command, "reboot") == 0 || strcmp(command, "filesystem-remove") == 0 ||
                                   strcmp(command, "ota-update") == 0 || strcmp(command, "factory-reset") == 0);
  }
};

}  // namespace esp32shell
