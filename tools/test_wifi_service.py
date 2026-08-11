"""Phase 2 source-contract tests for the portable Wi-Fi service."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
WIFI = ROOT / "firmware" / "arduino" / "esp32shell" / "wifi_service.h"
CORE = ROOT / "firmware" / "arduino" / "esp32shell" / "command_core.h"


class WifiServiceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.wifi = WIFI.read_text(encoding="utf-8")
        cls.core = CORE.read_text(encoding="utf-8")

    def test_state_machine_has_offline_connecting_and_connected_states(self):
        for state in ("Offline", "Connecting", "Connected"):
            self.assertIn(state, self.wifi)
        self.assertIn("kRetryIntervalMs = 10000", self.wifi)

    def test_credentials_are_bounded_and_not_exposed_by_status(self):
        self.assertIn("char ssid_[33]", self.wifi)
        self.assertIn("char password_[65]", self.wifi)
        self.assertNotIn("password_", self.wifi.split("int rssi")[0])

    def test_offline_behavior_and_retry_are_explicit(self):
        self.assertIn("if (!configured_)", self.wifi)
        self.assertIn("if (now < nextAttemptAt_)", self.wifi)
        self.assertIn("driver_.begin(ssid_, password_)", self.wifi)
        self.assertIn('"0.0.0.0"', self.wifi)

    def test_core_phase_two_commands_are_reserved(self):
        self.assertIn('"wifi-status"', self.core)
        self.assertIn('"wifi-config "', self.core)


if __name__ == "__main__":
    unittest.main()
