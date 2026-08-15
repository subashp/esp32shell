from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SERVICES = ROOT / "firmware" / "esp-idf" / "main" / "idf_command_services.h"
APP = ROOT / "firmware" / "esp-idf" / "main" / "idf_app_runtime.cpp"


class EspIdfServiceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.services = SERVICES.read_text(encoding="utf-8")
        cls.app = APP.read_text(encoding="utf-8")

    def test_services_use_real_idf_backends(self):
        for symbol in (
            "nvs_open",
            "nvs_set_str",
            "esp_reset_reason",
            "uxTaskGetNumberOfTasks",
            "gpio_set_level",
            "opendir",
            "fopen",
        ):
            self.assertIn(symbol, self.services)
        self.assertNotIn("service unavailable in ESP-IDF target", self.services)

    def test_apps_are_transport_independent_and_session_stable(self):
        self.assertIn("static AppRuntime& apps()", self.services)
        self.assertIn("SignedAppBundle::verify", self.app)
        self.assertIn("/littlefs/apps/%s.bundle", self.app)


if __name__ == "__main__":
    unittest.main()
