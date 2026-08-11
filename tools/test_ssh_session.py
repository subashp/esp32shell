from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SSH = ROOT / "firmware/arduino/esp32shell/ssh_session.h"
TRANSPORT = ROOT / "firmware/arduino/esp32shell/ssh_transport.h"
SUBMODULES = ROOT / ".gitmodules"
DEPENDENCIES = ROOT / "third_party/README.md"


class SshSessionContractTests(unittest.TestCase):
    def test_limits_and_timeout_are_bounded(self):
        source = SSH.read_text(encoding="utf-8")
        self.assertIn("kMaxSessions = 2", source)
        self.assertIn("kMaxLineLength = 96", source)
        self.assertIn("kIdleTimeoutMs = 300000", source)
        self.assertIn("expire(unsigned long now)", source)

    def test_session_lifecycle_is_explicit(self):
        source = SSH.read_text(encoding="utf-8")
        for method in ("open", "close", "touch", "active"):
            self.assertIn(f"{method}(", source)
        self.assertIn("active_[sessionId]", source)

    def test_transport_fails_closed_without_wolfssh(self):
        source = TRANSPORT.read_text(encoding="utf-8")
        self.assertIn("ESP32SHELL_ENABLE_WOLFSSH", source)
        self.assertIn("disabled-no-wolfssh", source)
        self.assertIn("acceptsPlaintextFallback", source)
        self.assertIn("return false", source)

    def test_official_dependencies_are_pinned(self):
        modules = SUBMODULES.read_text(encoding="utf-8")
        dependencies = DEPENDENCIES.read_text(encoding="utf-8")
        self.assertIn("github.com/wolfSSL/wolfssl.git", modules)
        self.assertIn("github.com/wolfSSL/wolfssh.git", modules)
        self.assertIn("ac01707f552c611fbd135cc723b2682b3e7f80f2", dependencies)
        self.assertIn("8643d7be841184f766374e3b0ed68ced6391543c", dependencies)


if __name__ == "__main__":
    unittest.main()
