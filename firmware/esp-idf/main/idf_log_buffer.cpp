#include "idf_log_buffer.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

namespace esp32shell_idf {
namespace {
constexpr size_t kMaxEntries = 64;
constexpr size_t kMaxMessageLength = 128;

char entries[kMaxEntries][kMaxMessageLength] = {};
size_t nextEntry = 0;
size_t entryCount = 0;
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
vprintf_like_t previousVprintf = nullptr;
bool installed = false;
volatile bool capturing = false;

int capture_vprintf(const char* format, va_list arguments) {
  va_list copy;
  va_copy(copy, arguments);
  char message[kMaxMessageLength] = {};
  const int formatted = std::vsnprintf(message, sizeof(message), format, copy);
  va_end(copy);
  if (!capturing) {
    capturing = true;
    portENTER_CRITICAL(&lock);
    size_t length = std::strlen(message);
    while (length > 0 && (message[length - 1] == '\n' || message[length - 1] == '\r')) --length;
    if (length > 0) {
      std::memcpy(entries[nextEntry], message, length < kMaxMessageLength - 1 ? length : kMaxMessageLength - 1);
      entries[nextEntry][length < kMaxMessageLength - 1 ? length : kMaxMessageLength - 1] = '\0';
      nextEntry = (nextEntry + 1) % kMaxEntries;
      if (entryCount < kMaxEntries) ++entryCount;
    }
    portEXIT_CRITICAL(&lock);
    capturing = false;
  }
  if (previousVprintf != nullptr) return previousVprintf(format, arguments);
  return formatted;
}
}  // namespace

void log_buffer_install() {
  if (installed) return;
  previousVprintf = esp_log_set_vprintf(capture_vprintf);
  installed = true;
}

void log_dump(esp32shell::CommandOutput& output) {
  char snapshot[kMaxEntries][kMaxMessageLength] = {};
  size_t count = 0;
  portENTER_CRITICAL(&lock);
  count = entryCount;
  const size_t first = (nextEntry + kMaxEntries - count) % kMaxEntries;
  for (size_t index = 0; index < count; ++index)
    std::strncpy(snapshot[index], entries[(first + index) % kMaxEntries], kMaxMessageLength - 1);
  portEXIT_CRITICAL(&lock);
  char header[48];
  std::snprintf(header, sizeof(header), "logs=count=%u capacity=%u", static_cast<unsigned>(count), static_cast<unsigned>(kMaxEntries));
  output.line(header);
  for (size_t index = 0; index < count; ++index) output.line(snapshot[index]);
}

}  // namespace esp32shell_idf
