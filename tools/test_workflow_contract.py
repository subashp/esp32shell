"""Source-contract tests for the Windows clone-to-board workflow."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / "tools" / "build_flash_monitor.ps1"
README = ROOT / "README.md"


class WorkflowContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workflow = WORKFLOW.read_text(encoding="utf-8")
        cls.readme = README.read_text(encoding="utf-8")

    def test_workflow_bootstraps_and_pins_dependencies(self):
        for text in ("ArduinoSA.CLI", "arduino-cli", "esp32:esp32", "3.3.11", "FlashSize=32M,PSRAM=opi"):
            self.assertIn(text, self.workflow)

    def test_workflow_supports_local_config_and_two_profiles(self):
        for text in ("Import-PowerShellDataFile", "ConfigPath", "WifiProfiles", "wifi-config $slot"):
            self.assertIn(text, self.workflow)
        self.assertIn("esp32shell.local.psd1", self.readme)

    def test_workflow_disconnects_uart(self):
        self.assertIn("$serial.Close()", self.workflow)
        self.assertIn("$serial.Dispose()", self.workflow)


if __name__ == "__main__":
    unittest.main()
