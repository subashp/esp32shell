from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
OTA = ROOT / "firmware/arduino/esp32shell/ota_policy.h"


class OtaPolicyContractTests(unittest.TestCase):
    def test_update_lifecycle_and_rollback_states_exist(self):
        source = OTA.read_text(encoding="utf-8")
        for state in ("Idle", "Downloading", "Verified", "PendingReboot", "RolledBack", "Failed"):
            self.assertIn(state, source)
        for method in ("begin", "verify", "markPendingReboot", "rollback"):
            self.assertIn(f"{method}(", source)

    def test_verification_precedes_reboot(self):
        source = OTA.read_text(encoding="utf-8")
        self.assertIn("state_ = valid ? OtaState::Verified : OtaState::Failed", source)
        self.assertIn("state_ == OtaState::Verified", source)


if __name__ == "__main__":
    unittest.main()
