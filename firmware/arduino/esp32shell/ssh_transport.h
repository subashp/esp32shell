#pragma once

#include "ssh_session.h"

// SSH is deliberately opt-in. The Arduino library index does not provide a
// maintained wolfSSH package, so the firmware must never silently fall back to
// plaintext TCP. Build with ESP32SHELL_ENABLE_WOLFSSH only after wolfSSH and
// its ESP32 port are supplied by the production build.
#if defined(ESP32SHELL_ENABLE_WOLFSSH)
#include <wolfssh/ssh.h>
#endif

namespace esp32shell {

class SshTransport {
 public:
  static constexpr bool compiledIn() {
#if defined(ESP32SHELL_ENABLE_WOLFSSH)
    return true;
#else
    return false;
#endif
  }

  const char* status() const {
    return compiledIn() ? "wolfssh-enabled" : "disabled-no-wolfssh";
  }

  bool acceptsPlaintextFallback() const { return false; }
};

}  // namespace esp32shell
