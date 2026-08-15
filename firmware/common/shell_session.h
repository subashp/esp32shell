#pragma once

#include <memory>
#include <new>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../arduino/esp32shell/command_core.h"
#include "shell_transport.h"

namespace esp32shell {

enum class ShellSessionResult { Closed, Error, CommandRequestedClose };

// Transport-neutral interactive session. UART logging is intentionally not a
// ShellTransport; a future UART shell can opt in by implementing the same
// byte transport and constructing this class.
class ShellSession {
 public:
  ShellSession(ShellTransport& transport, CommandCore& core,
               DeviceServices& services)
      : transport_(transport), core_(core), services_(services), output_(transport) {}

  ShellSessionResult run() {
    output_.line("esp32shell ssh shell");
    output_.line("Type 'help' for commands.");
    output_.prompt();

    std::unique_ptr<char[]> line(
        new (std::nothrow) char[CommandCore::kMaxCommandLength + 1]());
    if (!line) return ShellSessionResult::Error;

    size_t used = 0;
    bool previousWasCarriageReturn = false;
    uint8_t input[64] = {};
    while (transport_.connected()) {
      size_t received = 0;
      const ShellReadStatus status = transport_.read(input, sizeof(input), received);
      if (status == ShellReadStatus::WouldBlock) continue;
      if (status != ShellReadStatus::Data) return ShellSessionResult::Closed;
      for (size_t i = 0; i < received; ++i) {
        const char ch = static_cast<char>(input[i]);
        if (ch == '\r' || ch == '\n') {
          if (ch == '\n' && previousWasCarriageReturn) {
            previousWasCarriageReturn = false;
            continue;
          }
          previousWasCarriageReturn = ch == '\r';
          output_.raw("\r\n");
          line[used] = '\0';
          if (core_.dispatch(line.get(), output_, services_) ==
              CommandStatus::SessionClosed) {
            return ShellSessionResult::CommandRequestedClose;
          }
          used = 0;
          output_.prompt();
        } else if (ch == '\b' || ch == 0x7f) {
          if (used > 0) {
            --used;
            output_.raw("\b \b");
          }
        } else if (used < CommandCore::kMaxCommandLength) {
          previousWasCarriageReturn = false;
          line[used++] = ch;
          char echoed[2] = {ch, '\0'};
          output_.raw(echoed);
        } else {
          previousWasCarriageReturn = false;
          output_.line("error: command is too long");
          used = 0;
        }
      }
    }
    return ShellSessionResult::Closed;
  }

 private:
  class SessionOutput final : public CommandOutput {
   public:
    explicit SessionOutput(ShellTransport& transport) : transport_(transport) {}

    void line(const char* text) override {
      if (text == nullptr) return;
      raw(text);
      raw("\r\n");
    }

    void raw(const char* text) {
      if (text != nullptr) transport_.write(text, strlen(text));
    }

    void prompt() { raw("esp32shell>"); }

   private:
    ShellTransport& transport_;
  };

  ShellTransport& transport_;
  CommandCore& core_;
  DeviceServices& services_;
  SessionOutput output_;
};

}  // namespace esp32shell
