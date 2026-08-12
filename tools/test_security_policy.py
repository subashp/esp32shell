from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SECURITY = ROOT / "firmware/arduino/esp32shell/security_policy.h"
SKETCH = ROOT / "firmware/arduino/esp32shell/esp32shell.ino"


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

    def test_firmware_stores_password_digest_and_host_key_bytes(self):
        sketch = SKETCH.read_text(encoding="utf-8")
        for symbol in ("mbedtls_sha256", "ssh_pw_hash", "putBytes", "decodeHex", "verifySshPassword"):
            self.assertIn(symbol, sketch)
        policy = SECURITY.read_text(encoding="utf-8")
        self.assertIn("kPasswordDigestSize = 32", policy)
        self.assertIn("isProtectedKey", policy)
        self.assertIn("kMaxHostKeyBytes = 1984", sketch)
        self.assertIn("ASN.1 DER blob", sketch)


if __name__ == "__main__":
    unittest.main()
