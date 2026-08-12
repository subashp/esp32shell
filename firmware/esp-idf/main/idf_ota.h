#pragma once

#include "../../../arduino/esp32shell/command_core.h"

namespace esp32shell_idf {

// Marks a successfully booted image valid and exposes the actual ESP-IDF OTA
// slot state to the shared shell. Image download/signature verification stays
// in the signed updater that selects the inactive slot.
void ota_boot_validate();
void ota_status(esp32shell::CommandOutput& output);

}  // namespace esp32shell_idf
