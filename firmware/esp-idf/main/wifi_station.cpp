#include "wifi_station.h"

#include <cstring>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs.h"

namespace {
constexpr char kTag[] = "esp32shell-wifi";
constexpr char kNvsNamespace[] = "esp32shell";

bool read_string(nvs_handle_t nvs, const char* key, char* value, size_t capacity) {
  size_t length = capacity;
  return nvs_get_str(nvs, key, value, &length) == ESP_OK && value[0] != '\0';
}
}

extern "C" bool esp32shell_wifi_start() {
  nvs_handle_t nvs = 0;
  if (nvs_open(kNvsNamespace, NVS_READONLY, &nvs) != ESP_OK) {
    ESP_LOGW(kTag, "Wi-Fi disabled: NVS namespace is unavailable");
    return false;
  }
  char ssid[33] = {};
  char password[65] = {};
  const bool configured = read_string(nvs, "wifi_ssid", ssid, sizeof(ssid)) &&
                          read_string(nvs, "wifi_password", password, sizeof(password));
  nvs_close(nvs);
  if (!configured) {
    ESP_LOGW(kTag, "Wi-Fi disabled: configure wifi_ssid and wifi_password in NVS");
    return false;
  }

  ESP_ERROR_CHECK(esp_netif_init());
  esp_netif_create_default_wifi_sta();
  wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&initConfig));
  wifi_config_t config{};
  std::strncpy(reinterpret_cast<char*>(config.sta.ssid), ssid, sizeof(config.sta.ssid));
  std::strncpy(reinterpret_cast<char*>(config.sta.password), password, sizeof(config.sta.password));
  config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(kTag, "Wi-Fi station started for SSID %s", ssid);
  ESP_ERROR_CHECK(esp_wifi_connect());
  return true;
}
