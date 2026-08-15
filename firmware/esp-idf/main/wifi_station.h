#pragma once

#include <stddef.h>

struct Esp32shellWifiStatus {
  bool configured;
  bool connecting;
  unsigned activeSlot;
  char activeSsid[33];
  char ipAddress[16];
  int rssi;
};

// Starts the station from credentials provisioned in NVS. The station
// rotates through wifi_ssid_0/wifi_password_0 and _1 after bounded attempts.
extern "C" bool esp32shell_wifi_start();
extern "C" bool esp32shell_wifi_status(Esp32shellWifiStatus* status);
