#include "idf_ota.h"

#include <cstdio>

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "nvs.h"

extern "C" {
#include <wolfssl/wolfcrypt/ed25519.h>
#include <wolfssl/wolfcrypt/sha256.h>
}

namespace esp32shell_idf {
namespace {
constexpr char kTag[] = "esp32shell-ota";
constexpr char kNvsNamespace[] = "esp32shell";
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

bool ota_apply_signed_image(const uint8_t* image, size_t imageLength,
                            const uint8_t signature[64]) {
  if (image == nullptr || signature == nullptr || imageLength == 0) return false;
  nvs_handle_t nvs = 0;
  uint8_t publicKey[32] = {};
  size_t keyLength = sizeof(publicKey);
  const bool keyLoaded = nvs_open(kNvsNamespace, NVS_READONLY, &nvs) == ESP_OK &&
                         nvs_get_blob(nvs, "ota_public_key", publicKey, &keyLength) == ESP_OK &&
                         keyLength == sizeof(publicKey);
  if (nvs != 0) nvs_close(nvs);
  if (!keyLoaded) return false;

  uint8_t digest[32] = {};
  wc_Sha256 hash;
  if (wc_InitSha256(&hash) != 0 || wc_Sha256Update(&hash, image, imageLength) != 0 ||
      wc_Sha256Final(&hash, digest) != 0) return false;
  ed25519_key verifier{};
  int verified = 0;
  if (wc_ed25519_init(&verifier) != 0 ||
      wc_ed25519_import_public(publicKey, sizeof(publicKey), &verifier) != 0 ||
      wc_ed25519_verify_msg(signature, 64, digest, sizeof(digest), &verified, &verifier) != 0 ||
      verified != 1) {
    wc_ed25519_free(&verifier);
    ESP_LOGW(kTag, "signed OTA rejected");
    return false;
  }
  wc_ed25519_free(&verifier);

  const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
  if (target == nullptr || imageLength > target->size) return false;
  esp_ota_handle_t handle = 0;
  if (esp_ota_begin(target, imageLength, &handle) != ESP_OK) return false;
  const esp_err_t writeResult = esp_ota_write(handle, image, imageLength);
  const esp_err_t endResult = writeResult == ESP_OK ? esp_ota_end(handle) : esp_ota_abort(handle);
  if (writeResult != ESP_OK || endResult != ESP_OK || esp_ota_set_boot_partition(target) != ESP_OK) {
    ESP_LOGW(kTag, "signed OTA image was not selected");
    return false;
  }
  ESP_LOGI(kTag, "signed OTA image selected: %s", target->label);
  return true;
}

}  // namespace esp32shell_idf
