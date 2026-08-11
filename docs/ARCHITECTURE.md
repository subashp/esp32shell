# Architecture

## Boundary

The ESP32-S3 runs ESP-IDF FreeRTOS. `esp32shell` is a firmware command environment, not a Linux userland. It should provide ESP32-specific tools rather than attempt to drop in BusyBox.

## Shared command path

```text
USB serial ─┐
            ├── session adapter ── command parser ── command registry ── device services
SSH ────────┘
```

The command registry is the stable center. Serial and SSH are transports only.

## Initial command groups

- `system`: help, version, device-info, uptime, reboot
- `memory`: heap, psram
- `network`: wifi-status, wifi-config
- `tasks`: tasks, task-info
- `gpio`: modes, read, write, with an allowlist
- `filesystem`: list, read, write, remove
- `ota`: status, check, update, rollback

## Build strategy

The Arduino sketch is the initial hardware smoke test. The production SSH implementation is expected to use ESP-IDF or PlatformIO if the selected SSH server is not Arduino-compatible. The command contracts should remain portable between those builds.

## Security principles

- No default network credentials.
- Host keys persist in device storage.
- SSH is LAN-only during development.
- Destructive commands require authorization or confirmation.
- Credentials, keys, logs, and local device configuration stay outside Git.
