#pragma once

// The ESP-IDF SSH server is deliberately separate from the Arduino serial
// bring-up.  It becomes active only after the station has an IP address and
// only when NVS contains a username, SHA-256 password digest, and ASN.1 host
// private key.
extern "C" void esp32shell_ssh_start_after_wifi();
