#pragma once

#include <stddef.h>

namespace esp32shell {

struct SshSessionLimits {
  static constexpr size_t kMaxSessions = 2;
  static constexpr size_t kMaxLineLength = 2048;
  static constexpr unsigned long kIdleTimeoutMs = 300000;
};

class SshSessionManager {
 public:
  bool open(size_t sessionId, unsigned long now) {
    if (sessionId >= SshSessionLimits::kMaxSessions || active_[sessionId]) return false;
    active_[sessionId] = true;
    lastActivity_[sessionId] = now;
    return true;
  }
  void close(size_t sessionId) {
    if (sessionId < SshSessionLimits::kMaxSessions) active_[sessionId] = false;
  }
  void touch(size_t sessionId, unsigned long now) {
    if (sessionId < SshSessionLimits::kMaxSessions && active_[sessionId]) lastActivity_[sessionId] = now;
  }
  bool active(size_t sessionId) const {
    return sessionId < SshSessionLimits::kMaxSessions && active_[sessionId];
  }
  void expire(unsigned long now) {
    for (size_t i = 0; i < SshSessionLimits::kMaxSessions; ++i) {
      if (active_[i] && now - lastActivity_[i] >= SshSessionLimits::kIdleTimeoutMs) active_[i] = false;
    }
  }

 private:
  bool active_[SshSessionLimits::kMaxSessions] = {};
  unsigned long lastActivity_[SshSessionLimits::kMaxSessions] = {};
};

}  // namespace esp32shell
