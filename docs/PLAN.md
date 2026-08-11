# esp32shell implementation plan

This is the execution source of truth. Work proceeds one task at a time; each task must have an acceptance test before the next task starts.

## Phase 0 — Repository and toolchain baseline

- [x] Create repository structure and Git hygiene.
- [x] Add Arduino ESP32-S3 serial bring-up sketch.
- [ ] Verify the sketch compiles and flashes on the user's board.
- [ ] Record the working COM port and Arduino core version without committing machine-local state.

Acceptance: the board boots, prints a banner, and answers `help` and `device-info` over Serial Monitor.

## Phase 1 — Transport-neutral command core

- Separate command parsing and dispatch from serial I/O.
- Define command result and error conventions.
- Add `help`, `version`, `device-info`, `uptime`, `heap`, and `reboot`.
- Add host-side parser tests where practical.

Acceptance: all commands work over serial and malformed input returns a safe error.

## Phase 2 — Wi-Fi service

- Add station-mode provisioning without committing credentials.
- Report connection state, IP address, RSSI, and uptime.
- Add retry and offline behavior.

Acceptance: the board connects to a configured LAN and reports its address over serial.

## Phase 3 — SSH transport

- Evaluate `jimmyw/ssh_cli_server` and wolfSSH ESP32 examples.
- Select the smallest maintainable SSH server path.
- Forward SSH stdin/stdout to the shared command dispatcher.
- Add session limits, disconnect handling, and timeouts.

Acceptance: `ssh user@device-ip` opens `esp32shell>` and runs the same commands as serial.

## Phase 4 — Device toolbox

- Add Wi-Fi diagnostics.
- Add heap, PSRAM, uptime, reset-reason, and FreeRTOS task diagnostics.
- Add allowlisted GPIO inspection/control.
- Add logs and bounded output.

Acceptance: diagnostics are useful over SSH and cannot access unapproved pins or unbounded resources.

## Phase 5 — Filesystem and configuration

- Add LittleFS listing, read, write, and remove commands.
- Add NVS-backed configuration commands.
- Never print secrets in configuration output.

Acceptance: configuration survives reboot and filesystem operations are constrained to the device filesystem.

## Phase 6 — Security and recovery

- Persist SSH host keys.
- Require a non-default password or public-key authentication.
- Add command permissions and confirmation for destructive operations.
- Add safe reboot and recovery behavior.

Acceptance: unauthenticated access is rejected and dangerous commands require authorization.

## Phase 7 — OTA and release quality

- Add dual-partition OTA with rollback.
- Add firmware version and compatibility reporting.
- Add build instructions, smoke tests, and GitHub Actions.
- Publish source-only first; never commit credentials, private endpoints, local paths, build output, or device keys.

Acceptance: a signed or verified firmware update can be installed and rolled back safely.

## Execution rules

1. Complete one task before starting the next.
2. Run the task's acceptance test immediately.
3. Fix and retry failures before proceeding.
4. Keep this file synchronized with actual code and test status.
