#include "wifi_station.h"

#include <cstring>
#include <cstdio>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

namespace {
constexpr char kTag[] = "esp32shell-wifi";
constexpr char kNvsNamespace[] = "esp32shell";
constexpr TickType_t kAttemptTicks = pdMS_TO_TICKS(10000);
constexpr TickType_t kSwitchCooldownTicks = pdMS_TO_TICKS(500);

struct Profile { char ssid[33] = {}; char password[65] = {}; bool enabled = false; };
Profile profiles[2] = {};
size_t nextSlot = 0;
unsigned activeSlot = 0;
bool configured = false;
bool connecting = false;
bool connected = false;
TickType_t attemptStarted = 0;
esp_netif_t* stationNetif = nullptr;
TaskHandle_t failoverTask = nullptr;

bool readString(nvs_handle_t handle, const char* key, char* value, size_t capacity) {
  size_t length = capacity;
  return nvs_get_str(handle, key, value, &length) == ESP_OK && value[0] != '\0';
}

void loadProfiles() {
  nvs_handle_t handle = 0;
  if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) return;
  for (size_t slot = 0; slot < 2; ++slot) {
    char ssidKey[20] = {}; char passwordKey[24] = {};
    std::snprintf(ssidKey, sizeof(ssidKey), "wifi_ssid_%u", static_cast<unsigned>(slot));
    std::snprintf(passwordKey, sizeof(passwordKey), "wifi_password_%u", static_cast<unsigned>(slot));
    profiles[slot].enabled = readString(handle, ssidKey, profiles[slot].ssid, sizeof(profiles[slot].ssid)) &&
                             readString(handle, passwordKey, profiles[slot].password, sizeof(profiles[slot].password));
  }
  if (!profiles[0].enabled) {
    profiles[0].enabled = readString(handle, "wifi_ssid", profiles[0].ssid, sizeof(profiles[0].ssid)) &&
                          readString(handle, "wifi_password", profiles[0].password, sizeof(profiles[0].password));
  }
  nvs_close(handle);
  configured = profiles[0].enabled || profiles[1].enabled;
}

bool applyProfile(size_t slot) {
  if (slot >= 2 || !profiles[slot].enabled) return false;
  wifi_config_t config{};
  std::strncpy(reinterpret_cast<char*>(config.sta.ssid), profiles[slot].ssid, sizeof(config.sta.ssid));
  std::strncpy(reinterpret_cast<char*>(config.sta.password), profiles[slot].password, sizeof(config.sta.password));
  config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  const esp_err_t result = esp_wifi_set_config(WIFI_IF_STA, &config);
  if (result != ESP_OK) {
    ESP_LOGW(kTag, "profile %u could not be applied: %s", static_cast<unsigned>(slot), esp_err_to_name(result));
    return false;
  }
  activeSlot = static_cast<unsigned>(slot);
  nextSlot = (slot + 1) % 2;
  connecting = true;
  connected = false;
  attemptStarted = xTaskGetTickCount();
  ESP_LOGI(kTag, "trying Wi-Fi profile %u (%s)", static_cast<unsigned>(slot), profiles[slot].ssid);
  return esp_wifi_connect() == ESP_OK;
}

void wifi_event(void*, esp_event_base_t base, int32_t id, void*) {
  if (base != WIFI_EVENT) return;
  if (id == WIFI_EVENT_STA_DISCONNECTED) {
    connecting = false; connected = false;
  }
}

void wifi_failover_task(void*) {
  for (;;) {
    if (configured && !connected) {
      const TickType_t now = xTaskGetTickCount();
      if (!connecting && (attemptStarted == 0 || now - attemptStarted >= kSwitchCooldownTicks)) {
        for (size_t offset = 0; offset < 2; ++offset) {
          const size_t slot = (nextSlot + offset) % 2;
          if (applyProfile(slot)) break;
        }
      } else if (connecting && now - attemptStarted >= kAttemptTicks) {
        ESP_LOGW(kTag, "Wi-Fi profile %u timed out; rotating", activeSlot);
        esp_wifi_disconnect();
        connecting = false;
        attemptStarted = now;
        nextSlot = (activeSlot + 1) % 2;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}
}  // namespace

extern "C" bool esp32shell_wifi_start() {
  loadProfiles();
  if (!configured) {
    ESP_LOGW(kTag, "Wi-Fi disabled: no configured profiles");
    return false;
  }
  ESP_ERROR_CHECK(esp_netif_init());
  stationNetif = esp_netif_create_default_wifi_sta();
  wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&initConfig));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, nullptr));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());
  if (xTaskCreate(wifi_failover_task, "wifi-failover", 3072, nullptr, 4, &failoverTask) != pdPASS) {
    ESP_LOGE(kTag, "Wi-Fi failover task could not start"); return false;
  }
  ESP_LOGI(kTag, "Wi-Fi station started with %u configured profile(s)",
           static_cast<unsigned>(profiles[0].enabled + profiles[1].enabled));
  return true;
}

extern "C" bool esp32shell_wifi_status(Esp32shellWifiStatus* status) {
  if (status == nullptr) return false;
  *status = {};
  status->configured = configured;
  status->connecting = connecting;
  status->activeSlot = activeSlot;
  std::strncpy(status->activeSsid, profiles[activeSlot].ssid, sizeof(status->activeSsid) - 1);
  status->rssi = 0;
  wifi_ap_record_t record{};
  if (esp_wifi_sta_get_ap_info(&record) == ESP_OK) {
    status->rssi = record.rssi;
    connected = true;
    status->connecting = false;
  }
  if (stationNetif != nullptr) {
    esp_netif_ip_info_t info{};
    if (esp_netif_get_ip_info(stationNetif, &info) == ESP_OK) {
      std::snprintf(status->ipAddress, sizeof(status->ipAddress), IPSTR, IP2STR(&info.ip));
    }
  }
  if (status->ipAddress[0] == '\0') std::strcpy(status->ipAddress, "0.0.0.0");
  return true;
}
