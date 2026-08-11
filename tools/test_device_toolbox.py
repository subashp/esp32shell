from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
TOOLBOX = ROOT / "firmware/arduino/esp32shell/device_toolbox.h"
CORE = ROOT / "firmware/arduino/esp32shell/command_core.h"
SKETCH = ROOT / "firmware/arduino/esp32shell/esp32shell.ino"


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
        self.assertIn("count_ < kMaxEntries", source)

    def test_toolbox_commands_are_wired_to_firmware(self):
        core = CORE.read_text(encoding="utf-8")
        sketch = SKETCH.read_text(encoding="utf-8")
        for command in ("psram", "reset-reason", "tasks", "gpio-modes", "logs"):
            self.assertIn(f'"{command}"', core)
        for command in ("gpio-read ", "gpio-write "):
            self.assertIn(f'"{command}"', core)
        for symbol in ("uxTaskGetSystemState", "gpioAllowed", "digitalRead", "digitalWrite", "logs_.at"):
            self.assertIn(symbol, sketch)


if __name__ == "__main__":
    unittest.main()
