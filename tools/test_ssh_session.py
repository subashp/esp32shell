from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SSH = ROOT / "firmware/arduino/esp32shell/ssh_session.h"


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


if __name__ == "__main__":
    unittest.main()
