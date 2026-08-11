#pragma once

// Starts the station from credentials already provisioned in NVS. No
// credentials are embedded in the firmware image.
extern "C" bool esp32shell_wifi_start();
