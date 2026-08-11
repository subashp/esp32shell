from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class AppSupervisorContractTests(unittest.TestCase):
    def test_limits_and_lifecycle_are_bounded(self):
        source = (ROOT / "firmware/arduino/esp32shell/app_supervisor.h").read_text(encoding="utf-8")
        self.assertIn("kMaxApps = 4", source)
        self.assertIn("minimumFreeHeap", source)
        self.assertIn("markFailed", source)
        self.assertIn("AppLifecycle::Running", source)

    def test_diagnostics_uses_supervisor_limits(self):
        source = (ROOT / "firmware/arduino/esp32shell/esp32shell.ino").read_text(encoding="utf-8")
        self.assertIn('registerApp("diagnostics", {4096, 20000, 0})', source)
        self.assertIn('registerApp("led-blink", {3072, 20000, 0})', source)
        self.assertIn('xTaskCreatePinnedToCore(ledBlinkThunk', source)
        self.assertIn("supervisor_.canStart", source)
        self.assertIn("supervisor_.markRunning", source)


if __name__ == "__main__":
    unittest.main()
