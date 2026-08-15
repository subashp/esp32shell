# Architecture

## Runtime boundary

The supported hardware is an ESP32-S3-WROOM-2 running ESP-IDF FreeRTOS. The
device provides a firmware command environment and device services; it is not
a Linux userland and does not provide processes, BusyBox, or a general-purpose
shell.

There are two firmware targets:

- The Arduino target is the USB bootstrap and hardware-validation image. It
  provides the serial command prompt used to provision initial NVS values.
- The ESP-IDF target is the runtime image. UART is log-only, Wi-Fi starts from
  persisted NVS profiles, and authenticated wolfSSH provides the interactive
  shell and SFTP service.

The first-time transition is automated by
`tools/onboard_espidf.ps1`: flash Arduino, provision over COM4, then flash
ESP-IDF without erasing NVS.

## Shared command path

```text
Arduino USB shell ──┐
                    ├── ShellSession ── CommandCore ── DeviceServices
ESP-IDF SSH shell ──┘                                  ├─ Wi-Fi/NVS
                                                       ├─ LittleFS/apps
                                                       ├─ GPIO/diagnostics
                                                       └─ OTA status

ESP-IDF UART ─────────────── logs only
ESP-IDF SFTP ─────────────── LittleFS VFS (/littlefs)
```

`ShellSession` owns prompting, echo, line buffering, CR/LF handling, command
dispatch, and `exit`/`quit`. `ShellTransport` adapts the session to a stream.
The command core and device services do not depend on whether the caller is
the Arduino serial shell or an authenticated SSH session.

Asynchronous ESP-IDF logs are written to UART and copied into a bounded,
thread-safe in-memory ring. The `logs` command retrieves that ring through an
authenticated shell without injecting asynchronous output into the prompt.

## Device services

- `system`: help, version, device-info, uptime, reboot, reset-reason
- `memory`: heap, psram
- `network`: wifi-status, wifi-config, wifi-profiles, wifi-profile-clear
- `configuration`: config-list, config-get, config-set, config-clear
- `tasks`: bounded FreeRTOS task state and stack-watermark diagnostics
- `gpio`: allowlisted modes, read, and write
- `filesystem`: bounded LittleFS list/read/write/remove operations
- `apps`: app-list, app-run, app-stop, app-status
- `logs`: bounded device-log retrieval
- `ota`: OTA slot status and signed inactive-slot update API

The ESP-IDF implementation uses native NVS, LittleFS VFS, FreeRTOS, GPIO,
ESP-NETIF/Wi-Fi, wolfSSL/wolfSSH, and ESP-IDF OTA APIs. LittleFS and app
lifecycle state are device services, not shell-session state, so apps continue
running after an SSH disconnect and can be inspected by a later session.

## Wi-Fi and SSH startup

Two Wi-Fi profiles are loaded from NVS. The station attempts each configured
profile with bounded timeouts and rotates to the other profile after failure.
After `IP_EVENT_STA_GOT_IP`, the authenticated SSH listener starts on TCP port
22222. SSH requires a provisioned username, password digest, and ASN.1 private
host key; missing or invalid credentials fail closed.

The SSH shell and SFTP subsystem share the same authenticated wolfSSH server.
SFTP is rooted at `/littlefs`. Filesystem writes are bounded, reject traversal,
and app uploads are confined to `/apps`.

## Application model

Built-in apps are registered in a device-level `AppRuntime`. Current built-ins
are `diagnostics` and `led-blink`. Each app has bounded task resources,
heartbeat supervision, stack-watermark reporting, and a maximum restart count.
Signed LittleFS descriptors are verified with an Ed25519 public key before the
runtime accepts them.

The general custom-app authoring, signing, upload, and execution workflow is
not yet finalized; it remains a documented TODO rather than an implied
general-purpose executable format.

## OTA model

The partition table contains `app0` and `app1` OTA slots plus `otadata`. A
signed update is hashed and verified with the provisioned Ed25519 public key,
written only to the inactive slot, and selected for the next boot only after
all verification and write steps succeed. Boot validation calls
`esp_ota_mark_app_valid_cancel_rollback()` so an unvalidated image can be
rolled back by ESP-IDF. Live signed-update and rollback testing remains board
acceptance work.

## Security principles

- No default Wi-Fi or SSH credentials are compiled into the firmware.
- Initial credentials are provisioned over the local USB bootstrap path.
- SSH authentication and host keys remain protected in NVS.
- Generic configuration clearing preserves Wi-Fi and SSH access credentials.
- Filesystem paths, uploads, task resources, and app restarts are bounded.
- Destructive filesystem commands require explicit confirmation.
- Credentials, keys, logs, and machine-local configuration stay outside Git.
- Network exposure and client isolation depend on the configured Wi-Fi network;
  port 22222 should be treated as a development/service interface.
