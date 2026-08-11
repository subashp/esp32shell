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

Acceptance: host Wi-Fi contract passes; board LAN acceptance remains pending hardware access.

## Phase 3 — SSH transport

- [x] Add bounded SSH session limits and timeout policy.
- [ ] Evaluate `jimmyw/ssh_cli_server` and wolfSSH ESP32 examples.
- Select the smallest maintainable SSH server path.
- Forward SSH stdin/stdout to the shared command dispatcher.
- Add session limits, disconnect handling, and timeouts.

Acceptance: session policy is host-tested; live SSH forwarding remains pending library selection and hardware validation.

## Phase 4 — Device toolbox

- Add Wi-Fi diagnostics.
- Add heap, PSRAM, uptime, reset-reason, and FreeRTOS task diagnostics.
- Add allowlisted GPIO inspection/control.
- [x] Add allowlisted GPIO and bounded log policies.
- [ ] Add logs and bounded output implementation.

Acceptance: GPIO/log safety contracts are host-tested; live diagnostics remain pending transport integration.

## Phase 5 — Filesystem and configuration

- Add LittleFS listing, read, write, and remove commands.
- Add NVS-backed configuration commands.
- [x] Add constrained filesystem paths and secret-key output policy.
- Never print secrets in configuration output.

Acceptance: storage policies are host-tested; LittleFS/NVS persistence remains pending implementation.

## Phase 6 — Security and recovery

- Persist SSH host keys.
- Require a non-default password or public-key authentication.
- Add command permissions and confirmation for destructive operations.
- [x] Add password-strength and destructive-command confirmation policies.
- Add safe reboot and recovery behavior.

Acceptance: security policies are host-tested; live SSH authentication and persisted host keys remain pending.

## Phase 7 — OTA and release quality

- Add dual-partition OTA with rollback.
- Add firmware version and compatibility reporting.
- Add build instructions, smoke tests, and GitHub Actions.
- [x] Add verified OTA lifecycle and rollback state policy.
- Publish source-only first; never commit credentials, private endpoints, local paths, build output, or device keys.

Acceptance: OTA policy is host-tested; signed dual-partition installation remains pending device integration.

## Execution rules

1. Complete one task before starting the next.
2. Run the task's acceptance test immediately.
3. Fix and retry failures before proceeding.
4. Keep this file synchronized with actual code and test status.
