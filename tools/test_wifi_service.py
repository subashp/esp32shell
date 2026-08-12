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
        self.assertIn("char ssid[33]", self.wifi)
        self.assertIn("char password[65]", self.wifi)
        self.assertNotIn("password_", self.wifi.split("int rssi")[0])

    def test_offline_behavior_and_retry_are_explicit(self):
        self.assertIn("if (!configured_)", self.wifi)
        self.assertIn("if (now < nextAttemptAt_)", self.wifi)
        self.assertIn("driver_.begin(profiles_[slot].ssid, profiles_[slot].password)", self.wifi)
        self.assertIn("if (driver_.connecting())", self.wifi)
        self.assertIn('"0.0.0.0"', self.wifi)

    def test_core_phase_two_commands_are_reserved(self):
        self.assertIn('"wifi-status"', self.core)
        self.assertIn('"wifi-config "', self.core)
        self.assertIn('"wifi-profiles"', self.core)
        self.assertIn('"wifi-profile-clear"', self.core)

    def test_two_profile_failover_and_safe_metadata_are_present(self):
        self.assertIn("kMaxProfiles = 2", self.wifi)
        self.assertIn("nextProfile_ = (slot + 1) % kMaxProfiles", self.wifi)
        self.assertIn("activeSlot_ = slot", self.wifi)
        self.assertIn("const bool previousAttempt = state_ == WifiState::Connecting;", self.wifi)
        self.assertIn("if (previousAttempt) {", self.wifi)
        self.assertIn("kReconnectCooldownMs = 500", self.wifi)
        self.assertIn("cooldown_ = true", self.wifi)
        sketch = (ROOT / "firmware" / "arduino" / "esp32shell" / "esp32shell.ino").read_text(encoding="utf-8")
        self.assertIn("WiFi.disconnect(true, false)", sketch)
        self.assertIn("WiFi.mode(WIFI_OFF)", sketch)
        self.assertIn("WiFi.mode(WIFI_STA)", sketch)
        self.assertIn("WiFi.status() == WL_IDLE_STATUS", sketch)
        self.assertIn("Serial.write(static_cast<uint8_t>(c))", sketch)
        sketch = (ROOT / "firmware" / "arduino" / "esp32shell" / "esp32shell.ino").read_text(encoding="utf-8")
        self.assertIn("wifi_profile_%u", sketch)


if __name__ == "__main__":
    unittest.main()
