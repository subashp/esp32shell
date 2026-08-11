from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SECURITY = ROOT / "firmware/arduino/esp32shell/security_policy.h"


class SecurityPolicyContractTests(unittest.TestCase):
    def test_non_default_password_policy_is_present(self):
        source = SECURITY.read_text(encoding="utf-8")
        self.assertIn("strlen(password) < 12", source)
        self.assertIn("upper && lower && digit", source)

    def test_destructive_commands_require_confirmation(self):
        source = SECURITY.read_text(encoding="utf-8")
        for command in ("reboot", "filesystem-remove", "ota-update", "factory-reset"):
            self.assertIn(f'"{command}"', source)
        self.assertIn("requiresConfirmation", source)


if __name__ == "__main__":
    unittest.main()
