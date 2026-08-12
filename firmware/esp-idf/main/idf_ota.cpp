#include "idf_ota.h"

#include <cstdio>

#include "esp_log.h"
#include "esp_ota_ops.h"

namespace esp32shell_idf {
namespace {
constexpr char kTag[] = "esp32shell-ota";
}

void ota_boot_validate() {
  const esp_err_t result = esp_ota_mark_app_valid_cancel_rollback();
  if (result == ESP_OK) {
    ESP_LOGI(kTag, "OTA boot validation accepted");
  } else if (result != ESP_ERR_OTA_ROLLBACK_INVALID_STATE) {
    ESP_LOGW(kTag, "OTA boot validation unavailable: %s", esp_err_to_name(result));
  }
}

void ota_status(esp32shell::CommandOutput& output) {
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* boot = esp_ota_get_boot_partition();
  char line[128] = {};
  std::snprintf(line, sizeof(line), "ota-running=%s", running == nullptr ? "unknown" : running->label);
  output.line(line);
  std::snprintf(line, sizeof(line), "ota-boot=%s", boot == nullptr ? "unknown" : boot->label);
  output.line(line);
  output.line((running != nullptr && boot != nullptr && running->address == boot->address)
                  ? "ota-state=active"
                  : "ota-state=pending-reboot");
}

}  // namespace esp32shell_idf
