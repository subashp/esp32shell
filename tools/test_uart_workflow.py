from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "build_flash_monitor.ps1"


class UartWorkflowContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.script = SCRIPT.read_text(encoding="utf-8")

    def test_workflow_builds_with_required_board_options(self):
        self.assertIn('--board-options "FlashSize=32M,PSRAM=opi"', self.script)
        self.assertIn("--build-path $build", self.script)
        self.assertIn("--flash-size\\s+32MB", self.script)

    def test_workflow_uploads_com4_by_default_and_opens_monitor(self):
        self.assertIn('[string]$Port = "COM4"', self.script)
        self.assertIn("--port $Port", self.script)
        self.assertIn("--input-dir $build", self.script)
        self.assertIn("monitor --port $Port", self.script)
        self.assertIn("SkipUpload", self.script)
        self.assertIn("SkipMonitor", self.script)


if __name__ == "__main__":
    unittest.main()
