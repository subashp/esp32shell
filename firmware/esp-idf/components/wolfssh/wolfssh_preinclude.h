#pragma once

// wolfSSL's ESP-IDF settings intentionally define NO_FILESYSTEM for TLS.
// wolfSSH SFTP uses the separate ESP-IDF VFS adapter and must see its
// POSIX-compatible filesystem port branch instead.
#include "myFilesystem.h"
#define USE_OSE_API
// ESP-IDF's VFS exposes path chmod but not descriptor fchmod. SFTP mode
// updates are accepted as a no-op because LittleFS permissions are fixed.
static inline int esp32shell_fchmod(int fd, int mode) {
  (void)fd;
  (void)mode;
  return 0;
}
#define fchmod esp32shell_fchmod
#undef NO_FILESYSTEM
