#pragma once

#include <stddef.h>

namespace esp32shell {

class GpioPolicy {
 public:
  GpioPolicy(const int* allowedPins, size_t count) : pins_(allowedPins), count_(count) {}
  bool allowed(int pin) const {
    for (size_t i = 0; i < count_; ++i) if (pins_[i] == pin) return true;
    return false;
  }

 private:
  const int* pins_;
  size_t count_;
};

class BoundedLog {
 public:
  static constexpr size_t kMaxEntries = 64;
  static constexpr size_t kMaxMessageLength = 96;
  bool append(const char* message) {
    if (message == nullptr || count_ >= kMaxEntries) return false;
    size_t length = 0;
    while (message[length] != '\0' && length <= kMaxMessageLength) ++length;
    if (length > kMaxMessageLength) return false;
    ++count_;
    return true;
  }
  size_t count() const { return count_; }

 private:
  size_t count_ = 0;
};

}  // namespace esp32shell
