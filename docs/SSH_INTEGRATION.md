# SSH integration boundary

The firmware must use wolfSSH (or an equivalent maintained SSH implementation)
for encrypted transport. A raw `WiFiServer`/TCP shell is not an acceptable
fallback because it would expose credentials and shell traffic in plaintext.

The source contains `ssh_transport.h`, which is disabled by default and fails
the build if `ESP32SHELL_ENABLE_WOLFSSH` is enabled without the wolfSSH headers.
The Arduino library index currently has no maintained wolfSSH package, so the
production integration must either:

1. supply a reviewed wolfSSH ESP32 component through an ESP-IDF build, or
2. vendor a reviewed wolfSSH source snapshot outside the Arduino sketch and
   expose its include/library paths to the build.

When enabled, the adapter must authenticate before allocating a shell, use the
existing `SshSessionManager`, forward lines to `CommandCore`, enforce the
96-byte line and two-session limits, expire idle sessions, and reject all
plaintext fallback paths.
