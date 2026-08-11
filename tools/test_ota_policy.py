from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
OTA = ROOT / "firmware/arduino/esp32shell/ota_policy.h"
SERVICE = ROOT / "firmware/arduino/esp32shell/ota_service.h"


class OtaPolicyContractTests(unittest.TestCase):
    def test_update_lifecycle_and_rollback_states_exist(self):
        source = OTA.read_text(encoding="utf-8")
        for state in ("Idle", "Downloading", "Verified", "PendingReboot", "RolledBack", "Failed"):
            self.assertIn(state, source)
        for method in ("begin", "verify", "markPendingReboot", "rollback"):
            self.assertIn(f"{method}(", source)

    def test_verification_precedes_reboot(self):
        source = OTA.read_text(encoding="utf-8")
        self.assertIn("signatureValid", source)
        self.assertIn("hashValid", source)
        self.assertIn("state_ == OtaState::Verified", source)

    def test_ota_service_targets_inactive_slot_and_requires_signature(self):
        source = SERVICE.read_text(encoding="utf-8")
        for symbol in ("esp_ota_get_next_update_partition", "esp_ota_write", "esp_ota_end", "esp_ota_set_boot_partition"):
            self.assertIn(symbol, source)
        self.assertIn("verifier.verify", source)
        self.assertIn("!policy_.canBoot()", source)


if __name__ == "__main__":
    unittest.main()
