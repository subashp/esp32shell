#pragma once

#include "../../../arduino/esp32shell/command_core.h"

namespace esp32shell_idf {

void log_buffer_install();
void log_dump(esp32shell::CommandOutput& output);

}  // namespace esp32shell_idf
