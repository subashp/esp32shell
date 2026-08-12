# esp32shell implementation plan

This is the execution source of truth. Work proceeds one task at a time; each task must have an acceptance test before the next task starts.

## Phase 0 — Repository and toolchain baseline

- [x] Create repository structure and Git hygiene.
- [x] Add Arduino ESP32-S3 serial bring-up sketch.
- [x] Verify the sketch compiles and flashes on the user's board.
- [x] Record the working COM port (`COM4`) and Arduino core version (`3.3.11`) without committing machine-local state.

Acceptance: boot/banner acceptance is verified on COM4; manually run `help` and `device-info` and record the output as hardware validation.

## Phase 1 — Transport-neutral command core

- Separate command parsing and dispatch from serial I/O.
- Define command result and error conventions.
- Add `help`, `version`, `device-info`, `uptime`, `heap`, and `reboot`.
- [x] Add host-side parser tests where practical.

Acceptance: host parser contract passes; serial transport is booted on COM4, with command-response verification remaining a manual hardware test.

## Phase 2 — Wi-Fi service

- Add station-mode provisioning without committing credentials.
- Report connection state, IP address, RSSI, and uptime.
- [x] Add retry and offline behavior.
- [x] Persist Wi-Fi credentials in NVS and reload them at boot.
- [x] Add LittleFS-backed bounded file commands and configuration commands.

Acceptance: host Wi-Fi/storage contracts and ESP32 compile pass; reboot persistence and LAN acceptance remain hardware checks.

## Phase 3 — SSH transport

- [x] Add bounded SSH session limits and timeout policy.
- [x] Evaluate `jimmyw/ssh_cli_server` and wolfSSH ESP32 examples.
- [x] Add a fail-closed wolfSSH integration boundary; plaintext TCP fallback is rejected.
- [x] Select pinned wolfSSL/wolfSSH ESP-IDF components as the maintainable server path.
- [x] Start an authenticated server after the Wi-Fi IP event.
- [x] Forward SSH stdin/stdout to the shared command dispatcher.
- [x] Add bounded line input, disconnect handling, and server-task limits.

Acceptance: wolfSSH ESP-IDF image builds with SFTP enabled and the serial firmware is hardware-tested. Live LAN SSH authentication remains pending host-key provisioning and a Wi-Fi acceptance run.

## Phase 4 — Device toolbox

- Add Wi-Fi diagnostics.
- Add heap, PSRAM, uptime, reset-reason, and FreeRTOS task diagnostics.
- Add allowlisted GPIO inspection/control.
- [x] Add allowlisted GPIO and bounded log policies.
- [x] Add logs and bounded output implementation.
- [x] Wire PSRAM, reset-reason, task, GPIO, and log commands into the firmware.

Acceptance: GPIO/log safety contracts are host-tested; live diagnostics remain pending transport integration.

## Phase 5 — Filesystem and configuration

- Add LittleFS listing, read, write, and remove commands.
- Add NVS-backed configuration commands.
- [x] Add constrained filesystem paths and secret-key output policy.
- [x] Wire NVS configuration and LittleFS list/read/write/remove commands.
- Never print secrets in configuration output.

Acceptance: Arduino LittleFS/NVS persistence and COM4 file commands are implemented. The ESP-IDF target mounts its LittleFS partition at `/littlefs`; live persistence acceptance still requires flashing and exercising that image.

## Phase 6 — Security and recovery

- Persist SSH host keys.
- Require a non-default password or public-key authentication.
- Add command permissions and confirmation for destructive operations.
- [x] Add password-strength and destructive-command confirmation policies.
- Add safe reboot and recovery behavior.
- [x] Store SSH password digests and host-key bytes in protected NVS keys.

Acceptance: security policies are host-tested. ESP-IDF server startup fails closed without NVS username, digest, and ASN.1 host key; live SSH authentication remains pending provisioning and LAN testing.

## Phase 7 — OTA and release quality

- [x] Add dual-partition OTA with rollback.
- Add firmware version and compatibility reporting.
- Add build instructions, smoke tests, and GitHub Actions.
- [x] Add verified OTA lifecycle and rollback state policy.
- [x] Add inactive-slot OTA streaming and hash-plus-signature boot gating.
- [x] Add CI, partition validation, required-artifact checks, and release documentation.
- Publish source-only first; never commit credentials, private endpoints, local paths, build output, or device keys.

Acceptance: Arduino OTA policy and ESP-IDF boot-slot validation are implemented and build-tested. Signed OTA installation and rollback still require a real signed image, flashing, reboot, and rollback exercise on hardware.

## Phase 8 — Applications and remote delivery

- [x] Add a built-in diagnostics FreeRTOS app.
- [x] Add a built-in GPIO38 LED-blink FreeRTOS app.
- [x] Add bounded app registration, heap admission checks, and lifecycle supervision.
- [x] Add a signed, bounded, data-driven app-bundle format contract.
- [x] Add an authenticated, `/apps`-restricted file-upload command on the SSH shell path.
- [x] Mount LittleFS in the ESP-IDF SSH target and add the wolfSSH SFTP filesystem adapter.
- [x] Connect signed bundle verification to app installation and lifecycle launch.
- [x] Add UART provisioning for Wi-Fi, SSH password digest, and ASN.1 host-key storage.
- [ ] Validate live SSH/SFTP-equivalent upload and signed bundle execution over Wi-Fi.

Acceptance: built-in diagnostics and LED-blink lifecycle passed on COM4. Signed bundle verification and lifecycle execution are implemented; remote delivery and live execution remain pending valid host-key/Wi-Fi provisioning and network acceptance.

## Execution rules

1. Complete one task before starting the next.
2. Run the task's acceptance test immediately.
3. Fix and retry failures before proceeding.
4. Keep this file synchronized with actual code and test status.
