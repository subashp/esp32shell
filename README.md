# esp32shell

An experimental SSH-accessible command shell and device toolbox for the ESP32-S3 DevKitC-1.

## Current status

Phase 0 repository scaffold is in place. The Arduino bring-up sketch provides a minimal serial command shell so the command model can be tested before adding Wi-Fi and SSH.

## Hardware target

- ESP32-S3 DevKitC-1
- 32 MB Flash
- 16 MB PSRAM
- Onboard RGB LED on GPIO38 for the user's board revision

## First bring-up

Open `firmware/arduino/esp32shell/esp32shell.ino` in Arduino IDE.

Select:

- Board: `ESP32S3 Dev Module`
- Flash Size: `32MB`
- PSRAM: `OPI PSRAM`
- Port: the detected COM port

The serial shell is intentionally transport-independent in spirit. SSH will later use the same command dispatcher.

## Repository roadmap

See [docs/PLAN.md](docs/PLAN.md) and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
