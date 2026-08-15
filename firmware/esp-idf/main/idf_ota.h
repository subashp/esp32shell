#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../../arduino/esp32shell/command_core.h"

namespace esp32shell_idf {

// Marks a successfully booted image valid and exposes the actual ESP-IDF OTA
// slot state to the shared shell. Image download/signature verification stays
// in the signed updater that selects the inactive slot.
void ota_boot_validate();
void ota_status(esp32shell::CommandOutput& output);

// Verifies an in-memory image digest/signature, writes only the inactive OTA
// slot, and selects it for the next boot. The current image remains active if
// any verification or write step fails.
bool ota_apply_signed_image(const uint8_t* image, size_t imageLength,
                            const uint8_t signature[64]);

}  // namespace esp32shell_idf
