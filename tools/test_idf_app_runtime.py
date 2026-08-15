from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "firmware/esp-idf/main/idf_app_runtime.h"
SOURCE = ROOT / "firmware/esp-idf/main/idf_app_runtime.cpp"


class EspIdfAppRuntimeContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.source = SOURCE.read_text(encoding="utf-8")

    def test_builtins_and_device_level_registry_exist(self):
        self.assertIn('"diagnostics"', self.source)
        self.assertIn('"led-blink"', self.source)
        self.assertIn("kMaxApps = 4", self.header)
        self.assertIn("AppRuntime& apps()", (ROOT / "firmware/esp-idf/main/idf_command_services.h").read_text(encoding="utf-8"))

    def test_lifecycle_is_not_owned_by_shell_session(self):
        self.assertIn("xTaskCreate(taskThunk", self.source)
        self.assertIn("vTaskDelete(app->task)", self.source)
        self.assertIn("app-started=", self.source)
        self.assertIn("app-stopped=", self.source)
        self.assertIn("state=", self.source)

    def test_signed_bundles_remain_verified(self):
        self.assertIn("SignedAppBundle::verify", self.source)
        self.assertIn("app_signing_pub", self.source)
        self.assertIn("/littlefs/apps/%s.bundle", self.source)


if __name__ == "__main__":
    unittest.main()
