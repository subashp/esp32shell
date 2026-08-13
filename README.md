# esp32shell

An experimental UART command shell and device toolbox for the ESP32-S3-WROOM-2.
SSH transport work remains experimental follow-up work.

## Current status

Stable baseline: `stable-basic-shell-2026-08-12` (commit `71e8166`). The
firmware builds and has been flashed from Windows using Arduino CLI on `COM4`.
The baseline provides a responsive UART prompt, persisted dual-profile Wi-Fi,
basic app lifecycle commands, diagnostics, storage/configuration commands, and
addressable RGB LED blinking. Wi-Fi credentials and assigned IP addresses are
intentionally omitted from this document; configure them only through the
ignored local PowerShell data file or directly over UART.

SSH inbound/outbound support, SCP/SFTP interoperability, signed bundles, and
production OTA are not part of this stable baseline.

The implementation plans are local-only and ignored: `docs/PLAN.md`,
`docs/SHELL_PLAN.md`, and `docs/NEXT_PLAN.md`. This keeps machine-specific
workflow notes out of clones. The source architecture is documented in
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Hardware target

- ESP32-S3 DevKitC-1
- 32 MB Flash
- 16 MB PSRAM
- Onboard RGB LED on GPIO38 for the user's board revision

The onboard LED is an addressable RGB pixel. The `led-blink` app uses the
Arduino RGB LED API rather than treating GPIO38 as a normal digital output.

## Windows command-line setup

The supported command-line tool is Arduino CLI. The instructions below install the Espressif ESP32 platform version used by this repository and build the current sketch without Arduino IDE.

### Install Arduino CLI

Install the official Windows release from the [Arduino CLI installation documentation](https://arduino.github.io/arduino-cli/latest/installation/), or use WinGet if the package is available:

```powershell
winget search Arduino CLI
winget install --id ArduinoSA.CLI -e
arduino-cli version
```

The workflow script uses a per-user temporary CLI state directory under `%TEMP%\esp32shell-arduino-cli`, which avoids permission problems with a locked `AppData\Local\Arduino15` directory. It bootstraps the pinned core there when needed. Override it with `-CliStateRoot` if a persistent location is preferred.

Initialize the CLI manually, if desired, and install Arduino-ESP32 3.3.11:

```powershell
arduino-cli config init --overwrite
arduino-cli config add board_manager.additional_urls `
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.11
arduino-cli board listall "ESP32S3 Dev Module"
```

The CLI configuration and downloaded cores are machine-local. The workflow keeps them outside the repository by default; do not commit them to this repository.

### Clone-to-board quick start

After cloning on Windows, the workflow script can install Arduino CLI through
WinGet (when available), bootstrap the pinned ESP32 core, build, verify, flash,
run UART acceptance commands, disconnect COM4, and return to PowerShell:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
Copy-Item .\config\esp32shell.local.psd1.example .\config\esp32shell.local.psd1
# Edit the local file and replace both example SSIDs/passwords.
.\tools\build_flash_monitor.cmd -Clean -ConfigPath .\config\esp32shell.local.psd1
```

The script uses the required `FlashSize=32M,PSRAM=opi` settings and keeps CLI
state under `%TEMP%`. If the automatic install cannot run, install Arduino CLI
with `winget install --id ArduinoSA.CLI -e` or the official installer, reopen
PowerShell, and rerun the same command. If CLI state reports a Windows symlink
access error, add `-ResetCliState` once.

The local PowerShell data file is the supported credential input. It may contain
one or two `WifiProfiles` entries; passwords are sent over the USB UART to the
already-flashed Arduino firmware and are not printed. Never commit the `.local.psd1`
file. A data file cannot put the board into download mode, bypass a locked COM
port, or physically reset the board; for those cases put the ESP32-S3 in download
mode and close any other serial monitor before rerunning.

### Build the firmware

From the repository root:

```powershell
$sketch = (Resolve-Path 'firmware\arduino\esp32shell').Path
New-Item -ItemType Directory -Force .build\arduino | Out-Null

arduino-cli compile `
  --fqbn esp32:esp32:esp32s3 `
  --board-options "FlashSize=32M,PSRAM=opi" `
  --build-path .build\arduino `
  --warnings all `
  $sketch
```

Target configuration:

- Board: `ESP32S3 Dev Module`
- Flash: `32MB`
- PSRAM: `OPI PSRAM`
- Partition table: `firmware/arduino/esp32shell/partitions.csv`

The `--board-options` values are important. Without `FlashSize=32M`, Arduino CLI defaults this generic board to 4MB and produces a boot image that rejects the 32MB partition table. Confirm the generated `.build\arduino\flash_args` contains `--flash-size 32MB` before flashing.

### Flash COM4

```powershell
arduino-cli board list
Get-CimInstance Win32_SerialPort |
  Select-Object DeviceID,Description,PNPDeviceID

arduino-cli upload `
  --fqbn esp32:esp32:esp32s3 `
  --port COM4 `
  --input-dir .build\arduino
```

Before reflashing after a partition-size error, identify the physical flash size without erasing it:

```powershell
esptool --port COM4 flash-id
```

If `flash-id` reports 4MB, use a 4MB partition scheme or verify the board/module variant. If it reports 32MB, rebuild with `--board-options "FlashSize=32M,PSRAM=opi"` and reflash the bootloader, partition table, and application.

If the ESP32-S3 does not enumerate, hold `BOOT`, tap `RESET`, release `BOOT`, and repeat `arduino-cli board list`. A data-capable USB cable and the board's native USB connector are required.

### Open the serial shell

```powershell
arduino-cli monitor --port COM4 --config baudrate=115200
```

Initial acceptance commands:

```text
help
version
device-info
uptime
heap
wifi-status
wifi-profiles
app-list
app-run led-blink
app-status
app-stop led-blink
```

## Arduino IDE alternative

Open `firmware/arduino/esp32shell/esp32shell.ino` in Arduino IDE.

Select:

- Board: `ESP32S3 Dev Module`
- Flash Size: `32MB`
- PSRAM: `OPI PSRAM`
- Port: `COM4`

The command core is transport-neutral, but the stable baseline exposes it over
UART only. SSH uses the same dispatcher in experimental ESP-IDF work and is not
covered by the baseline acceptance workflow.

### Experimental SSH provisioning

If you are explicitly testing the experimental ESP-IDF SSH image, provision Wi-Fi and authentication into
the shared NVS namespace through the Arduino firmware on COM4. This is separate
from the stable UART-only baseline:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\provision_ssh.ps1 `
  -Port COM4 `
  -WifiSsid '<wifi-ssid>' `
  -SshUsername 'root'
```

When password parameters are omitted, the script prompts securely without
echoing the values or placing them in the PowerShell command line:

```text
Wi-Fi password: *****************
SSH password: ***************
Provisioning completed on COM4. The DER host key remains in protected NVS; no key file was kept.
```

The script creates a temporary 2048-bit RSA DER host key, stores it in NVS,
and removes the temporary file. Credentials and key material are never stored
in the repository. Flash the ESP-IDF image without erasing NVS, then monitor
COM4 for the assigned IP and connect with an SSH/SFTP client on port `22222`.

### One-command UART workflow

The reusable PowerShell workflow builds with the required 32MB/OPI options, verifies the generated artifacts and flash size, uploads to `COM4`, runs UART command tests using terminal output, closes the COM port, and returns:

```powershell
.\tools\build_flash_monitor.cmd -Clean
```

The `.cmd` launcher applies a process-scoped PowerShell execution-policy bypass, which is useful on Windows machines that block direct `.ps1` execution. You can also invoke the PowerShell script directly when policy permits.

Useful variants:

```powershell
.\tools\build_flash_monitor.ps1 -OpenMonitor             # test, then attach interactively
.\tools\build_flash_monitor.ps1 -SkipUpload -SkipTest     # build and verify only
.\tools\build_flash_monitor.ps1 -Clean -ResetCliState     # rebuild CLI state if bootstrap state is damaged
.\tools\build_flash_monitor.ps1 -Port COM4                # reuse the build directory
.\tools\build_flash_monitor.ps1 -ResetCliState            # repair a stale CLI state directory
```

## Repository roadmap

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md). If present locally, use
`docs/NEXT_PLAN.md` for the current execution checklist; the older plan files
are archived and intentionally ignored.
