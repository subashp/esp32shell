#pragma once

#include <stddef.h>
#include <string.h>

namespace esp32shell {

class SecurityPolicy {
 public:
  static constexpr size_t kPasswordDigestSize = 32;
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
  static bool isProtectedKey(const char* key) {
    return key != nullptr && (strcmp(key, "ssh_password") == 0 || strcmp(key, "ssh_pw_hash") == 0 ||
                               strcmp(key, "ssh_host_key") == 0);
  }
};

}  // namespace esp32shell
