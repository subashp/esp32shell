# SSH integration

The ESP-IDF target uses pinned wolfSSL/wolfSSH components. It waits for the
station IP event, listens on TCP port 22222, authenticates before creating the
shell, and forwards bounded input to the shared `CommandCore`. Plain TCP
fallback is never enabled.

The server requires these values in NVS namespace `esp32shell`:

- `ssh_username`: username string.
- `ssh_pw_hash`: 32-byte SHA-256 password digest.
- `ssh_host_key`: ASN.1 private-key blob.

If any value is absent or malformed, the server fails closed. Build the target
with `tools/build_espidf_ssh.cmd`; the generated image is separate from the
Arduino COM4 firmware.

The authenticated shell also exposes the path-restricted command
`fs-write /apps/<name> <content>`. This is the current authenticated upload
endpoint. It is not yet a drop-in SFTP server: the ESP-IDF target still needs a
LittleFS VFS mount and live Wi-Fi validation before remote app upload can be
accepted.
