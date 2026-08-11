# Release and CI contract

Every change must pass the host contract tests and the partition validator.
The Arduino CI job compiles the ESP32-S3 target with the pinned Arduino-ESP32
3.3.11 core and explicit 32MB flash/OPI PSRAM options. It does not claim live
COM, SSH, or signed-OTA acceptance; those require hardware and are recorded
separately.

Required release artifacts are the ELF, map, application binary, partition
binary, and `flash_args`. The partition validator requires two 8MB OTA app
slots, NVS, OTA metadata, coredump, and LittleFS within the 32MB flash range.
