#pragma once

namespace esp32shell {

enum class OtaState { Idle, Downloading, Verified, PendingReboot, RolledBack, Failed };

class OtaPolicy {
 public:
  void begin() { state_ = OtaState::Downloading; }
  void verify(bool valid) { state_ = valid ? OtaState::Verified : OtaState::Failed; }
  void markPendingReboot() { if (state_ == OtaState::Verified) state_ = OtaState::PendingReboot; }
  void rollback() { state_ = OtaState::RolledBack; }
  OtaState state() const { return state_; }

 private:
  OtaState state_ = OtaState::Idle;
};

}  // namespace esp32shell
