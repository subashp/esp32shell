# esp32shell

## What this repository is about

`esp32shell` is a Windows-first command shell and device toolbox for an
ESP32-S3. The stable firmware provides a UART-accessible prompt with device
diagnostics, persisted Wi-Fi profiles, configuration/storage commands, and a
built-in RGB LED demonstration app.

The stable workflow is serial/UART based. Local planning documents are ignored
and are not required to build or use the repository.

## Supported hardware

The current supported target is:

- ESP32-S3-WROOM-2 module
- ESP32-S3 DevKitC-1 board configuration
- 32 MB flash
- 16 MB OPI PSRAM
- Addressable RGB LED on GPIO38 for the supported board revision

The Arduino partition table is at
`firmware/arduino/esp32shell/partitions.csv`.

## Setup

Windows PowerShell is the supported development environment.

### External tools

Install these tools before starting:

- Git
- Arduino CLI
- A USB data cable and the board's USB serial driver, if Windows does not
  enumerate the board automatically

Install Arduino CLI from the [official installation documentation](https://arduino.github.io/arduino-cli/latest/installation/),
or with WinGet:

```powershell
winget install --id ArduinoSA.CLI -e
arduino-cli version
```

The workflow bootstraps Arduino-ESP32 core `3.3.11` into a machine-local
temporary directory. It does not store toolchains or credentials in this
repository.

### Clone and configure

```powershell
git clone git@github.com:subashp/esp32shell.git
cd esp32shell
Set-ExecutionPolicy -Scope Process Bypass
Copy-Item .\config\esp32shell.local.psd1.example .\config\esp32shell.local.psd1
```

Edit the ignored `config\esp32shell.local.psd1` file with the Wi-Fi profiles
you want to provision. Never commit that file.

Put the board into download mode when required, close other serial monitors,
and identify its port:

```powershell
arduino-cli board list
```

### Automated build, flash, and UART validation

The recommended development workflow performs dependency setup, compilation,
artifact checks, flashing, UART command validation, and serial-port cleanup:

```powershell
.\tools\build_flash_monitor.cmd -Clean -ConfigPath .\config\esp32shell.local.psd1 -Port COM4
```

Replace `COM4` with the port reported by `arduino-cli board list`. The command
returns to PowerShell after validation. To attach an interactive monitor after
the workflow completes:

```powershell
.\tools\build_flash_monitor.ps1 -OpenMonitor -Port COM4
```

If Arduino CLI cannot be installed automatically, install it manually with
WinGet or the official installer, reopen PowerShell, and rerun the workflow.

### Manual Arduino CLI setup

```powershell
arduino-cli config init --overwrite
arduino-cli config add board_manager.additional_urls `
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.11
```

The board configuration is `ESP32S3 Dev Module` with `FlashSize=32M` and
`PSRAM=opi`. Omitting the 32 MB flash option can create a boot image with an
incorrect partition layout.

## Usage

### Build manually

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

### Flash manually

```powershell
$port = "COM4"
arduino-cli upload `
  --fqbn esp32:esp32:esp32s3 `
  --port $port `
  --input-dir .build\arduino
```

For a flash-size diagnostic, use:

```powershell
esptool --port $port flash-id
```

If the board is not listed, use a known data-capable cable, try a direct USB
port, hold `BOOT`, tap `RESET`, release `BOOT`, and run `arduino-cli board list`
again.

### Open the shell

```powershell
arduino-cli monitor --port $port --config baudrate=115200
```

The stable prompt supports commands including:

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

The `led-blink` app drives the supported addressable RGB LED. Wi-Fi passwords
and other sensitive values are stored on the device or in ignored local
configuration and are not part of the repository documentation.

### SSH client diagnostics

The experimental SSH workflow uses port `22222`. Keep OpenSSH diagnostic
output out of the interactive shell by redirecting standard error to a local
file:

```powershell
ssh -vvv -tt -p 22222 root@<board-ip> 2> ssh-debug.log
```

Replace `<board-ip>` with the address currently reported by the board. The
shell remains on the terminal, while OpenSSH `debug3` output is written to
`ssh-debug.log`.
