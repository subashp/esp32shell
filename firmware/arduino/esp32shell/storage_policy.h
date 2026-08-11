#pragma once

#include <stddef.h>
#include <string.h>

namespace esp32shell {

class FilesystemPolicy {
 public:
  static constexpr size_t kMaxPathLength = 64;
  static bool validPath(const char* path) {
    if (path == nullptr || path[0] != '/' || strlen(path) > kMaxPathLength) return false;
    return strstr(path, "..") == nullptr && strstr(path, "//") == nullptr;
  }
};

class ConfigOutputPolicy {
 public:
  static bool isSecretKey(const char* key) {
    return key != nullptr && (strcmp(key, "wifi_password") == 0 || strcmp(key, "ssh_password") == 0 ||
                              strcmp(key, "ssh_pw_hash") == 0 || strcmp(key, "ssh_private_key") == 0 ||
                              strcmp(key, "ssh_host_key") == 0);
  }
};

}  // namespace esp32shell
