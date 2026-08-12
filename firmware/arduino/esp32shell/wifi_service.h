#pragma once

#include <stddef.h>
#include <string.h>

namespace esp32shell {

enum class WifiState { Offline, Connecting, Connected };

class WifiDriver {
 public:
  virtual ~WifiDriver() = default;
  virtual void begin(const char* ssid, const char* password) = 0;
  virtual bool connected() const = 0;
  virtual void disconnect() = 0;
  virtual int rssi() const = 0;
  virtual const char* ipAddress() const = 0;
};

class WifiService {
 public:
  static constexpr size_t kMaxProfiles = 2;
  static constexpr unsigned long kRetryIntervalMs = 10000;

  explicit WifiService(WifiDriver& driver) : driver_(driver) {}

  bool configure(const char* ssid, const char* password, unsigned long now) {
    return configure(0, ssid, password, now);
  }

  bool configure(size_t slot, const char* ssid, const char* password, unsigned long now) {
    if (slot >= kMaxProfiles || ssid == nullptr || password == nullptr || ssid[0] == '\0') return false;
    strncpy(profiles_[slot].ssid, ssid, sizeof(profiles_[slot].ssid) - 1);
    profiles_[slot].ssid[sizeof(profiles_[slot].ssid) - 1] = '\0';
    strncpy(profiles_[slot].password, password, sizeof(profiles_[slot].password) - 1);
    profiles_[slot].password[sizeof(profiles_[slot].password) - 1] = '\0';
    profiles_[slot].enabled = true;
    configured_ = anyConfigured();
    nextAttemptAt_ = now;
    state_ = WifiState::Offline;
    return true;
  }

  void clear() {
    driver_.disconnect();
    for (size_t slot = 0; slot < kMaxProfiles; ++slot) clear(slot);
    configured_ = false;
    state_ = WifiState::Offline;
  }

  void clear(size_t slot) {
    if (slot >= kMaxProfiles) return;
    const bool wasActive = activeSlot_ == slot;
    profiles_[slot] = {};
    configured_ = anyConfigured();
    if (wasActive) {
      driver_.disconnect();
      state_ = WifiState::Offline;
      nextAttemptAt_ = 0;
    }
  }

  void tick(unsigned long now) {
    if (!configured_) {
      state_ = WifiState::Offline;
      return;
    }
    if (driver_.connected()) {
      state_ = WifiState::Connected;
      return;
    }
    const bool previousAttempt = state_ == WifiState::Connecting;
    state_ = WifiState::Offline;
    if (now < nextAttemptAt_) return;
    const size_t slot = nextProfile();
    if (slot >= kMaxProfiles) { state_ = WifiState::Offline; return; }
    // Arduino-ESP32 rejects WiFi.begin() while the previous station attempt is
    // still active. End the bounded attempt before switching profiles.
    if (previousAttempt) driver_.disconnect();
    driver_.begin(profiles_[slot].ssid, profiles_[slot].password);
    activeSlot_ = slot;
    state_ = WifiState::Connecting;
    nextAttemptAt_ = now + kRetryIntervalMs;
    nextProfile_ = (slot + 1) % kMaxProfiles;
  }

  WifiState state() const { return state_; }
  bool configured() const { return configured_; }
  bool configured(size_t slot) const { return slot < kMaxProfiles && profiles_[slot].enabled; }
  const char* ssid(size_t slot) const { return slot < kMaxProfiles ? profiles_[slot].ssid : ""; }
  size_t activeSlot() const { return activeSlot_; }
  int rssi() const { return driver_.connected() ? driver_.rssi() : 0; }
  const char* ipAddress() const { return driver_.connected() ? driver_.ipAddress() : "0.0.0.0"; }

 private:
  WifiDriver& driver_;
  struct Profile {
    char ssid[33] = {};
    char password[65] = {};
    bool enabled = false;
  };
  bool anyConfigured() const {
    for (const auto& profile : profiles_) if (profile.enabled) return true;
    return false;
  }
  size_t nextProfile() const {
    for (size_t offset = 0; offset < kMaxProfiles; ++offset) {
      const size_t slot = (nextProfile_ + offset) % kMaxProfiles;
      if (profiles_[slot].enabled) return slot;
    }
    return kMaxProfiles;
  }
  Profile profiles_[kMaxProfiles] = {};
  unsigned long nextAttemptAt_ = 0;
  WifiState state_ = WifiState::Offline;
  bool configured_ = false;
  size_t nextProfile_ = 0;
  size_t activeSlot_ = 0;
};

}  // namespace esp32shell
