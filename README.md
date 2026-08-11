# esp32shell

An experimental SSH-accessible command shell and device toolbox for the ESP32-S3 DevKitC-1.

## Current status

The ESP32-S3 sketch builds and has been flashed from Windows using Arduino CLI on `COM4`. Boot/banner acceptance is verified, including the 32MB partition table and coredump partition. The current firmware provides the transport-neutral command core, serial shell, Wi-Fi state/retry path, and bounded policy modules for the later SSH, storage, security, diagnostics, and OTA work.

The remaining work is tracked in [docs/SHELL_PLAN.md](docs/SHELL_PLAN.md). Machine-local build output and credentials must remain untracked.

## Hardware target

- ESP32-S3 DevKitC-1
- 32 MB Flash
- 16 MB PSRAM
- Onboard RGB LED on GPIO38 for the user's board revision

## Windows command-line setup

The supported command-line tool is Arduino CLI. The instructions below install the Espressif ESP32 platform version used by this repository and build the current sketch without Arduino IDE.

### Install Arduino CLI

Install the official Windows release from the [Arduino CLI installation documentation](https://arduino.github.io/arduino-cli/latest/installation/), or use WinGet if the package is available:

```powershell
winget search Arduino CLI
winget install --id ArduinoSA.CLI -e
arduino-cli version
```

Initialize the CLI and install Arduino-ESP32 3.3.11:

```powershell
arduino-cli config init --overwrite
arduino-cli config add board_manager.additional_urls `
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.11
arduino-cli board listall "ESP32S3 Dev Module"
```

The CLI configuration and downloaded cores are user-local. Do not commit them to this repository.

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
```

## Arduino IDE alternative

Open `firmware/arduino/esp32shell/esp32shell.ino` in Arduino IDE.

Select:

- Board: `ESP32S3 Dev Module`
- Flash Size: `32MB`
- PSRAM: `OPI PSRAM`
- Port: `COM4`

The serial shell is intentionally transport-independent in spirit. SSH will later use the same command dispatcher.

## Repository roadmap

See [docs/SHELL_PLAN.md](docs/SHELL_PLAN.md), [docs/PLAN.md](docs/PLAN.md), and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
