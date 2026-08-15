from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "firmware/esp-idf/main/wifi_station.cpp"
HEADER = ROOT / "firmware/esp-idf/main/wifi_station.h"


class EspIdfWifiStationContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text(encoding="utf-8")
        cls.header = HEADER.read_text(encoding="utf-8")

    def test_two_profiles_and_bounded_failover_exist(self):
        self.assertIn("Profile profiles[2]", self.source)
        self.assertIn("kAttemptTicks", self.source)
        self.assertIn("wifi_failover_task", self.source)
        self.assertIn("nextSlot = (activeSlot + 1) % 2", self.source)

    def test_status_exposes_safe_connection_metadata(self):
        self.assertIn("Esp32shellWifiStatus", self.header)
        self.assertIn("activeSlot", self.header)
        self.assertIn("esp32shell_wifi_status", self.source)


if __name__ == "__main__":
    unittest.main()
