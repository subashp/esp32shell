#pragma once

#include <stddef.h>
#include <string.h>

namespace esp32shell {

enum class AppLifecycle { Stopped, Starting, Running, Failed };

struct AppResourceLimits {
  size_t stackBytes;
  size_t minimumFreeHeap;
  unsigned long maxRuntimeMs;
};

class AppSupervisor {
 public:
  static constexpr size_t kMaxApps = 4;

  bool registerApp(const char* name, AppResourceLimits limits) {
    if (name == nullptr || name[0] == '\0' || find(name) != nullptr) return false;
    for (auto& slot : slots_) {
      if (slot.name[0] == '\0') {
        strncpy(slot.name, name, sizeof(slot.name) - 1);
        slot.limits = limits;
        slot.state = AppLifecycle::Stopped;
        return true;
      }
    }
    return false;
  }

  bool canStart(const char* name, size_t freeHeap) const {
    const Slot* slot = find(name);
    return slot != nullptr && slot->state == AppLifecycle::Stopped &&
           freeHeap >= slot->limits.minimumFreeHeap;
  }
  void markStarting(const char* name) { setState(name, AppLifecycle::Starting); }
  void markRunning(const char* name) { setState(name, AppLifecycle::Running); }
  void markFailed(const char* name) { setState(name, AppLifecycle::Failed); }
  void markStopped(const char* name) { setState(name, AppLifecycle::Stopped); }
  AppLifecycle state(const char* name) const {
    const Slot* slot = find(name);
    return slot == nullptr ? AppLifecycle::Failed : slot->state;
  }
  const AppResourceLimits* limits(const char* name) const {
    const Slot* slot = find(name);
    return slot == nullptr ? nullptr : &slot->limits;
  }

 private:
  struct Slot {
    char name[24] = {};
    AppResourceLimits limits = {};
    AppLifecycle state = AppLifecycle::Stopped;
  };
  Slot slots_[kMaxApps] = {};

  Slot* find(const char* name) {
    for (auto& slot : slots_) if (slot.name[0] != '\0' && strcmp(slot.name, name) == 0) return &slot;
    return nullptr;
  }
  const Slot* find(const char* name) const {
    for (const auto& slot : slots_) if (slot.name[0] != '\0' && strcmp(slot.name, name) == 0) return &slot;
    return nullptr;
  }
  void setState(const char* name, AppLifecycle state) {
    Slot* slot = find(name);
    if (slot != nullptr) slot->state = state;
  }
};

}  // namespace esp32shell
