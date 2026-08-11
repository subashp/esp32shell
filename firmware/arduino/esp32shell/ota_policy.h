#pragma once

namespace esp32shell {

enum class OtaState { Idle, Downloading, Verified, PendingReboot, BootValidated, RolledBack, Failed, SignatureRejected };

class OtaPolicy {
 public:
  void begin() { state_ = OtaState::Downloading; }
  void verify(bool hashValid, bool signatureValid) {
    state_ = !signatureValid ? OtaState::SignatureRejected :
             (hashValid ? OtaState::Verified : OtaState::Failed);
  }
  void markPendingReboot() { if (state_ == OtaState::Verified) state_ = OtaState::PendingReboot; }
  void markBootValidated() { if (state_ == OtaState::PendingReboot) state_ = OtaState::BootValidated; }
  void rollback() { state_ = OtaState::RolledBack; }
  bool canBoot() const { return state_ == OtaState::Verified || state_ == OtaState::PendingReboot; }
  OtaState state() const { return state_; }

 private:
  OtaState state_ = OtaState::Idle;
};

}  // namespace esp32shell
