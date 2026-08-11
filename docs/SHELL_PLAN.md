# ESP32Shell implementation and bring-up plan

This is the detailed execution plan for turning the Arduino serial shell into a usable ESP32-S3 shell over USB serial and LAN SSH. Each task requires a focused test or documented hardware acceptance procedure before the next task begins.

## 1. Current verified baseline

- Hardware: ESP32-S3 DevKitC-1 variant, 32MB Flash, 16MB PSRAM, RGB LED on GPIO38.
- Arduino target: `ESP32S3 Dev Module`.
- Arduino-ESP32 core: 3.3.11.
- Serial port: `COM4`.
- Firmware: builds and has been flashed through Arduino CLI.
- Boot acceptance: verified on COM4; the banner appears, the 32MB partition table loads, and no coredump-partition warning appears after the latest flash.
- Build output: `.build/arduino/`; machine-local and never committed.
- Source partition table: `firmware/arduino/esp32shell/partitions.csv`.

The standard acceptance loop is:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3 --board-options "FlashSize=32M,PSRAM=opi" --build-path .build\arduino --warnings all firmware\arduino\esp32shell
arduino-cli upload --fqbn esp32:esp32:esp32s3 --port COM4 --input-dir .build\arduino
arduino-cli monitor --port COM4 --config baudrate=115200
```

The same workflow is available as a PowerShell script:

```powershell
.\tools\build_flash_monitor.ps1 -Clean
```

It compiles with `FlashSize=32M,PSRAM=opi`, verifies `flash_args` and the firmware/partition binaries, uploads to `COM4`, and opens `arduino-cli monitor`. Use `-SkipUpload -SkipMonitor` for build verification only.

Do not place Wi-Fi credentials, SSH passwords, private keys, host keys, or local logs in the repository.

## 2. Build and flash contract

### 2.1 Arduino CLI installation

Install Arduino CLI from the official Windows release or WinGet:

```powershell
winget search Arduino CLI
winget install --id ArduinoSA.CLI -e
arduino-cli version
```

Configure the Espressif package index and pin the core version:

```powershell
arduino-cli config init --overwrite
arduino-cli config add board_manager.additional_urls `
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.11
arduino-cli board listall "ESP32S3 Dev Module"
arduino-cli board details --fqbn esp32:esp32:esp32s3
```

The CLI configuration and downloaded platform are user-local. The repository documents the required version but does not vendor the toolchain.

### 2.2 Reproducible build

The build script should verify Arduino CLI, the pinned ESP32 core, the partition table, and the expected `.elf`, `.bin`, partition binary, and map outputs. It should compile with `--warnings all` into `.build/arduino`, report size, and fail if required board options are unavailable.

Target options are 32MB Flash, OPI PSRAM, and an OTA-capable partition scheme. The generic ESP32-S3 board defaults to 4MB unless `--board-options "FlashSize=32M,PSRAM=opi"` is supplied. The generated `.build\arduino\flash_args` must contain `--flash-size 32MB` before flashing. Board option names can vary by core release, so the script must validate them with `arduino-cli board details` rather than silently guessing.

### 2.3 Flash and serial acceptance

```powershell
arduino-cli board list
Get-CimInstance Win32_SerialPort |
  Select-Object DeviceID,Description,PNPDeviceID

arduino-cli upload --fqbn esp32:esp32:esp32s3 --port COM4 --input-dir .build\arduino
arduino-cli monitor --port COM4 --config baudrate=115200
```

Acceptance commands:

```text
help
version
device-info
uptime
heap
wifi-status
```

The boot/banner portion of this acceptance is verified. Command-response checks are intentionally manual hardware tests because they require the physical board and serial monitor; record successful output in the task commit or release notes rather than fabricating host-test evidence.

If no port appears, use a data-capable USB cable, connect to the native USB connector, and enter ROM download mode by holding `BOOT`, tapping `RESET`, and releasing `BOOT`.

Before reflashing after a partition-size error, identify the physical flash size without erasing it:

```powershell
esptool --port COM4 flash-id
```

If it reports 4MB, the 32MB partition table cannot be used; verify the board/module variant or switch to a 4MB partition layout. If it reports 32MB, rebuild with the explicit 32MB board options and reflash the bootloader, partition table, and application.

## 3. Phase 2: Wi-Fi and persistent configuration

Implement station mode with no credentials compiled into firmware, explicit `wifi-config <ssid> <password>` provisioning over a trusted serial session, bounded credentials, `wifi-status` reporting state/IP/RSSI without the password, retry timers, and clean disconnect/reconfiguration.

Use Arduino `Preferences`/ESP32 NVS for Wi-Fi credentials, SSH account metadata, host-key metadata or protected host-key bytes, firmware configuration, and boot-validation state. Use LittleFS for bounded logs and non-secret files. Reject overlong paths and paths containing `..` or duplicate separators.

Acceptance: configure Wi-Fi over COM4, reboot, confirm reconnection without reprovisioning, confirm no password leakage, clear configuration, and verify offline behavior. Test LittleFS create/read/list/remove, reboot persistence, traversal rejection, and bounded output.

## 4. Phase 3: SSH transport

Evaluate wolfSSH's ESP32 example/component path as the primary candidate. Avoid an unmaintained Arduino SSH library as the security boundary. If Arduino-only integration is not maintainable, move the production build to ESP-IDF with Arduino as a component while retaining the shared command core.

```text
USB Serial ─┐
            ├── session adapter ── line parser ── CommandCore ── services
SSH/TCP  ───┘
```

SSH must use the same `CommandCore` as serial. Implement LAN-only binding during development, at most two sessions, bounded lines, idle timeout, disconnect cleanup, authentication before shell allocation, and no unauthenticated fallback.

Acceptance:

```powershell
ssh user@<device-ip>
```

Run `help`, `device-info`, `wifi-status`, and `uptime`; then test a second session, idle timeout, disconnect, malformed input, and an overlong line.

## 5. Phase 4: Device toolbox

Add bounded `device-info`, `heap`, `psram`, `uptime`, `reset-reason`, `tasks`, `wifi-status`, `gpio-modes`, `gpio-read`, `gpio-write`, and `logs` commands. GPIO access must be allowlisted. Task enumeration must not block the shell. Logs use a fixed-size ring buffer. Output is bounded by command and session limits. Reboot and GPIO writes require explicit confirmation where appropriate.

Acceptance is performed over both COM4 and SSH after SSH is available.

## 6. Phase 5: Filesystem and NVS shell commands

Implement `config-list`, `config-get`, `config-set`, `config-clear --confirm`, `fs-list`, `fs-read`, `fs-write`, and `fs-remove`. Redact Wi-Fi passwords, SSH passwords, private keys, and tokens. Configuration writes should be atomic where practical and report failures without exposing values.

Acceptance includes reboot persistence, malformed paths, bounded files, and secret-redaction checks.

## 7. Phase 6: Authentication and recovery

Require a non-default SSH password on first use, store only a password verifier or migrate to public-key authentication, generate and persist an SSH host key in protected storage, add command authorization levels, require confirmation for reboot/file removal/factory reset/OTA, and keep serial recovery usable if networking fails.

Do not enable irreversible Secure Boot or flash-encryption fuses until serial recovery, key backup, and signed OTA have been tested on a sacrificial device.

Acceptance: unauthenticated SSH is rejected, default credentials are rejected, host identity survives reboot, destructive commands require authorization and confirmation, and serial recovery works without Wi-Fi.

## 8. Phase 7: Dual-partition OTA and release quality

The partition table reserves `app0`, `app1`, and `otadata`. Download to the inactive slot, verify size/hash/compatibility/signature, mark it pending, reboot, run startup self-tests, mark success valid, and mark failure invalid so the bootloader rolls back.

For production, use ESP-IDF OTA APIs or Arduino as an ESP-IDF component so rollback and secure-boot integration are explicit. Add firmware version/compatibility commands, signed-image instructions, CI compile/host-test jobs, partition validation, and release manifests containing version, target, size, hash, and signature metadata.

Acceptance: install a valid update, confirm the new version boots and is marked valid, force a failed self-test, confirm rollback, and reject a modified image.

## 9. Test matrix

| Area | Host validation | Device validation |
|---|---|---|
| Command core | parser/contract tests | COM4 commands |
| Wi-Fi | state/retry tests | LAN connection and reconnect |
| NVS | adapter tests | reboot persistence |
| LittleFS | path/bounds tests | file operations and reboot |
| SSH | session-limit tests | authenticated SSH on LAN |
| Toolbox | allowlist/bounds tests | diagnostics over COM4 and SSH |
| Security | policy tests | rejected unauthenticated access |
| OTA | lifecycle tests | signed update and rollback |

Every task should add or update a focused test and one self-contained commit. Build artifacts belong under `.build/` and must remain ignored; credentials, private keys, device logs, and local configuration must never be committed.
