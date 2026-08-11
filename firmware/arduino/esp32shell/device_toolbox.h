#pragma once

#include <stddef.h>
#include <string.h>

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
    if (message == nullptr) return false;
    size_t length = 0;
    while (message[length] != '\0' && length <= kMaxMessageLength) ++length;
    if (length > kMaxMessageLength) return false;
    strncpy(entries_[next_], message, kMaxMessageLength);
    entries_[next_][kMaxMessageLength] = '\0';
    next_ = (next_ + 1) % kMaxEntries;
    if (count_ < kMaxEntries) ++count_;
    return true;
  }
  size_t count() const { return count_; }
  const char* at(size_t index) const {
    if (index >= count_) return "";
    const size_t first = count_ == kMaxEntries ? next_ : 0;
    return entries_[(first + index) % kMaxEntries];
  }

 private:
  size_t count_ = 0;
  size_t next_ = 0;
  char entries_[kMaxEntries][kMaxMessageLength + 1] = {};
};

}  // namespace esp32shell
