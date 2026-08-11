from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
TOOLBOX = ROOT / "firmware/arduino/esp32shell/device_toolbox.h"


class DeviceToolboxContractTests(unittest.TestCase):
    def test_gpio_is_allowlisted(self):
        source = TOOLBOX.read_text(encoding="utf-8")
        self.assertIn("class GpioPolicy", source)
        self.assertIn("bool allowed(int pin)", source)
        self.assertIn("pins_[i] == pin", source)

    def test_logs_are_bounded(self):
        source = TOOLBOX.read_text(encoding="utf-8")
        self.assertIn("kMaxEntries = 64", source)
        self.assertIn("kMaxMessageLength = 96", source)
        self.assertIn("count_ >= kMaxEntries", source)


if __name__ == "__main__":
    unittest.main()
