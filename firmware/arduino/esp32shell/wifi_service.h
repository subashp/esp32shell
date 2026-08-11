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
  static constexpr unsigned long kRetryIntervalMs = 10000;

  explicit WifiService(WifiDriver& driver) : driver_(driver) {}

  bool configure(const char* ssid, const char* password, unsigned long now) {
    if (ssid == nullptr || password == nullptr || ssid[0] == '\0') return false;
    strncpy(ssid_, ssid, sizeof(ssid_) - 1);
    ssid_[sizeof(ssid_) - 1] = '\0';
    strncpy(password_, password, sizeof(password_) - 1);
    password_[sizeof(password_) - 1] = '\0';
    configured_ = true;
    nextAttemptAt_ = now;
    state_ = WifiState::Offline;
    return true;
  }

  void clear() {
    driver_.disconnect();
    ssid_[0] = '\0';
    password_[0] = '\0';
    configured_ = false;
    state_ = WifiState::Offline;
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
    state_ = WifiState::Offline;
    if (now < nextAttemptAt_) return;
    driver_.begin(ssid_, password_);
    state_ = WifiState::Connecting;
    nextAttemptAt_ = now + kRetryIntervalMs;
  }

  WifiState state() const { return state_; }
  bool configured() const { return configured_; }
  int rssi() const { return driver_.connected() ? driver_.rssi() : 0; }
  const char* ipAddress() const { return driver_.connected() ? driver_.ipAddress() : "0.0.0.0"; }

 private:
  WifiDriver& driver_;
  char ssid_[33] = {};
  char password_[65] = {};
  unsigned long nextAttemptAt_ = 0;
  WifiState state_ = WifiState::Offline;
  bool configured_ = false;
};

}  // namespace esp32shell
