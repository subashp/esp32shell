#pragma once

#include <stddef.h>
#include <stdint.h>

namespace esp32shell {

enum class ShellReadStatus { Data, WouldBlock, Closed, Error };

// A transport carries bytes only. It does not interpret commands or render a
// prompt, which keeps SSH and any future UART shell interchangeable.
class ShellTransport {
 public:
  virtual ~ShellTransport() = default;
  virtual ShellReadStatus read(uint8_t* buffer, size_t capacity,
                               size_t& length) = 0;
  virtual bool write(const char* data, size_t length) = 0;
  virtual bool connected() const = 0;
};

}  // namespace esp32shell
