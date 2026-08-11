#pragma once

#include "ota_policy.h"
#include <esp_ota_ops.h>

namespace esp32shell {

class SignatureVerifier {
 public:
  virtual ~SignatureVerifier() = default;
  virtual bool verify(const uint8_t* digest, size_t digestLength,
                      const uint8_t* signature, size_t signatureLength) = 0;
};

class OtaSlotManager {
 public:
  bool begin(size_t imageSize) {
    target_ = esp_ota_get_next_update_partition(nullptr);
    if (target_ == nullptr || imageSize == 0 || imageSize > target_->size) return false;
    policy_.begin();
    return esp_ota_begin(target_, imageSize, &handle_) == ESP_OK;
  }
  bool write(const uint8_t* data, size_t length) {
    return handle_ != 0 && data != nullptr && length > 0 && esp_ota_write(handle_, data, length) == ESP_OK;
  }
  bool finalize(bool imageHashValid, SignatureVerifier& verifier,
                const uint8_t* digest, size_t digestLength,
                const uint8_t* signature, size_t signatureLength) {
    if (handle_ == 0 || target_ == nullptr) return false;
    const bool signatureValid = verifier.verify(digest, digestLength, signature, signatureLength);
    if (esp_ota_end(handle_) != ESP_OK) { handle_ = 0; policy_.verify(false, signatureValid); return false; }
    handle_ = 0;
    policy_.verify(imageHashValid, signatureValid);
    if (!policy_.canBoot()) return false;
    policy_.markPendingReboot();
    return esp_ota_set_boot_partition(target_) == ESP_OK;
  }
  const char* state() const {
    switch (policy_.state()) {
      case OtaState::Idle: return "idle";
      case OtaState::Downloading: return "downloading";
      case OtaState::Verified: return "verified";
      case OtaState::PendingReboot: return "pending-reboot";
      case OtaState::BootValidated: return "boot-validated";
      case OtaState::RolledBack: return "rolled-back";
      case OtaState::SignatureRejected: return "signature-rejected";
      default: return "failed";
    }
  }
  OtaPolicy& policy() { return policy_; }

 private:
  const esp_partition_t* target_ = nullptr;
  esp_ota_handle_t handle_ = 0;
  OtaPolicy policy_;
};

}  // namespace esp32shell
