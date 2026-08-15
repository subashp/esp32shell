# esp32shell

## What this repository is about

`esp32shell` is a Windows-first command shell and device toolbox for the
ESP32-S3. It provides a shared command core, persistent Wi-Fi profiles,
LittleFS storage, built-in device apps, diagnostics, and an authenticated SSH
shell in the ESP-IDF image.

The Arduino image is the USB bootstrap and validation image. The ESP-IDF
image is the runtime image: UART is log-only and SSH is the interactive shell.

## Supported hardware

- Windows 11 development host
- ESP32-S3-WROOM-2
- ESP32-S3 DevKit-style board configuration
- 32 MB flash
- 16 MB OPI PSRAM
- Addressable RGB LED on GPIO38 for the supported board revision

## Required tools

Install these tools on Windows 11. The Arduino workflow automatically installs
Arduino CLI and the pinned Arduino-ESP32 core when they are missing.

### Git

```powershell
winget install --id Git.Git -e
git --version
```

### Arduino CLI

```powershell
winget install --id ArduinoSA.CLI -e
arduino-cli version
```

The scripts also accept an existing `arduino-cli.exe` on `PATH` and bootstrap
Arduino-ESP32 core `3.3.11` into `.build\arduino-cli`.

### ESP-IDF

The ESP-IDF SSH build scripts expect ESP-IDF 5.5.5 installed at
`C:\Espressif\v5.5.5\esp-idf` with the ESP32-S3 toolchain. Install the matching
release using Espressif's Windows installer, or install ESP-IDF manually from
the official documentation:

<https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32/get-started/windows-setup.html>

The ESP-IDF build scripts configure the tool paths themselves. A USB data
cable and the board's Windows USB serial driver are also required.

## Clone and build

```powershell
git clone git@github.com:subashp/esp32shell.git
cd esp32shell
Set-ExecutionPolicy -Scope Process Bypass
Copy-Item .\config\esp32shell.local.psd1.example .\config\esp32shell.local.psd1
```

Edit the ignored `config\esp32shell.local.psd1` and enter one or two local
Wi-Fi profiles. Never commit this file.

For the Arduino bootstrap image, build, flash, and validate with:

```powershell
.\tools\build_flash_monitor.cmd -Clean `
  -ConfigPath .\config\esp32shell.local.psd1 `
  -Port COM4
```

Replace `COM4` with the port shown by:

```powershell
arduino-cli board list
```

The script installs dependencies, compiles, validates artifacts, flashes the
image, provisions configured Wi-Fi profiles, runs UART command tests, and
disconnects from the port. Add `-OpenMonitor` to remain attached to the UART
monitor after validation.

To build the ESP-IDF image directly:

```powershell
cmd /c .\tools\build_espidf_ssh.cmd --clean
```

## Flash and onboard Wi-Fi/SSH

The ESP-IDF image intentionally has no UART shell. Therefore a fresh board
must first run the Arduino bootstrap image so credentials can be written to
NVS over USB. The complete first-time workflow is automated here:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\onboard_espidf.ps1 `
  -Port COM4 `
  -ConfigPath .\config\esp32shell.local.psd1
```

This workflow:

1. Builds and flashes the Arduino bootstrap.
2. Writes the configured Wi-Fi profile over COM4.
3. Prompts for the Wi-Fi password and SSH password, then provisions the SSH
   username and temporary protected host key.
4. Builds and flashes the ESP-IDF image without erasing NVS.

The SSH password must be at least 12 characters and contain uppercase,
lowercase, and numeric characters. The script does not keep the generated host
key file.

For a board already running the Arduino image, SSH provisioning can be run
directly:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\provision_ssh.ps1 `
  -Port COM4 `
  -WifiSsid '<local-wifi-ssid>' `
  -SshUsername 'root'
```

## Watch UART logs and connect over SSH

The ESP-IDF image prints boot, Wi-Fi, SSH, crash, and application logs over
UART. It does not provide a UART command prompt:

```powershell
arduino-cli monitor --port COM4 --config baudrate=115200
```

After `wifi-status` or the UART log reports the board IP, connect from a
Windows PowerShell terminal:

```powershell
ssh -tt -p 22222 root@<board-ip>
```

For SSH protocol diagnostics, keep debug output out of the shell display:

```powershell
ssh -vvv -tt -p 22222 root@<board-ip> 2> ssh-debug.log
```

The authenticated SSH shell and SFTP service are available only after the
ESP-IDF image has valid provisioned credentials and has acquired a Wi-Fi IP.

## Basic commands

Run these in the authenticated SSH shell. `logs` is also available through
the shared command service; asynchronous device logs remain on UART.

| Command | Summary |
|---|---|
| `help` | List available commands. |
| `version` | Show firmware version. |
| `device-info` | Show chip, flash, PSRAM, and ESP-IDF details. |
| `uptime` | Show device uptime. |
| `heap` | Show free and minimum heap. |
| `psram` | Show free PSRAM. |
| `tasks` | Show bounded FreeRTOS task diagnostics. |
| `reset-reason` | Show the last reset reason. |
| `wifi-status` | Show connection state, profile, IP, and RSSI. |
| `wifi-profiles` | List configured Wi-Fi profile SSIDs. |
| `wifi-config` | Configure and persist a Wi-Fi profile. |
| `wifi-profile-clear` | Remove a profile after `--confirm`. |
| `config-list` | List persisted configuration keys without secrets. |
| `config-get` | Read a non-secret configuration value. |
| `config-set` | Set an allowlisted configuration value. |
| `config-clear` | Clear non-critical configuration while preserving credentials. |
| `fs-list` | List LittleFS files and directories. |
| `fs-read` | Read a bounded LittleFS file. |
| `fs-write` | Write a bounded file under `/apps`. |
| `fs-remove` | Remove an app file after `--confirm`. |
| `gpio-modes` | Show allowlisted GPIO pins. |
| `gpio-read` | Read an allowlisted GPIO pin. |
| `gpio-write` | Write an allowlisted GPIO pin. |
| `logs` | Retrieve the bounded device log ring. |
| `ota-status` | Show active and boot OTA slots. |
| `app-list` | List built-in and signed app descriptors. |
| `app-run` | Start a built-in or verified app. |
| `app-stop` | Stop a running app. |
| `app-status` | Show app state, health, and restart information. |
| `reboot` | Restart the device. |
| `exit` / `quit` | Close the current SSH shell session. |

## Writing and flashing your own app

TODO: the signed LittleFS app bundle format and end-to-end custom app build,
sign, upload, and launch workflow are not yet finalized. The current runtime
supports the built-in `diagnostics` and `led-blink` apps plus verification of a
limited signed descriptor format; use `app-list`, `app-run`, and `app-status`
for those built-in apps while this workflow is completed.
