#pragma once

// wolfSSL's ESP-IDF settings define NO_FILESYSTEM for TLS. wolfSSH SFTP uses
// the ESP-IDF VFS adapter and must select its user-filesystem port instead.
#include_next <wolfssh/settings.h>
#undef NO_FILESYSTEM
