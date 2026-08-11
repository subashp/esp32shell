#include "littlefs_mount.h"

#include "esp_log.h"
#include "esp_littlefs.h"

namespace {
constexpr char kTag[] = "esp32shell-littlefs";
constexpr char kPartitionLabel[] = "littlefs";
constexpr char kBasePath[] = "/littlefs";
}

extern "C" bool esp32shell_littlefs_mount() {
  esp_vfs_littlefs_conf_t config{};
  config.base_path = kBasePath;
  config.partition_label = kPartitionLabel;
  config.format_if_mount_failed = false;
  config.dont_mount = false;
  const esp_err_t result = esp_vfs_littlefs_register(&config);
  if (result != ESP_OK) {
    ESP_LOGE(kTag, "LittleFS mount failed: %s", esp_err_to_name(result));
    return false;
  }
  size_t total = 0;
  size_t used = 0;
  if (esp_littlefs_info(kPartitionLabel, &total, &used) == ESP_OK) {
    ESP_LOGI(kTag, "LittleFS mounted at %s (%u/%u bytes used)", kBasePath,
             static_cast<unsigned>(used), static_cast<unsigned>(total));
  }
  return true;
}
