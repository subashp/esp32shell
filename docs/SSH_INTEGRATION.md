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

Provision the values over the Arduino UART before flashing the ESP-IDF image:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\provision_ssh.ps1 `
  -Port COM4 `
  -WifiSsid 'SRP-WiFi' `
  -WifiPassword '<wifi-password>' `
  -SshUsername 'esp32shell' `
  -SshPassword '<separate-strong-ssh-password>'
```

The helper generates a temporary 2048-bit RSA ASN.1/DER host key, writes its
hex form through the bounded UART command path, and deletes the temporary key
file. Do not use the Wi-Fi password as the SSH password.

The authenticated shell also exposes the path-restricted command
`fs-write /apps/<name> <content>`. The ESP-IDF target additionally enables the
wolfSSH SFTP subsystem and roots its default path at `/littlefs`, making a
standard SFTP client the intended file-transfer path. LittleFS is mounted at
that path during startup.

Live SFTP interoperability, persistence, and signed bundle execution still
require flashing this ESP-IDF image, provisioning a valid ASN.1 host private key
and password digest in NVS, and completing a Wi-Fi acceptance run.
