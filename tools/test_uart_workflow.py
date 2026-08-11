from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "build_flash_monitor.ps1"
LAUNCHER = ROOT / "tools" / "build_flash_monitor.cmd"


class UartWorkflowContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.script = SCRIPT.read_text(encoding="utf-8")

    def test_workflow_builds_with_required_board_options(self):
        self.assertIn('--board-options "FlashSize=32M,PSRAM=opi"', self.script)
        self.assertIn("arduino-cli.yaml", self.script)
        self.assertIn("directories:", self.script)
        self.assertIn('"core", "install"', self.script)
        self.assertIn("ProgramFiles", self.script)
        self.assertIn("CliStateRoot", self.script)
        self.assertIn("GetTempPath", self.script)
        self.assertIn("ResetCliState", self.script)
        self.assertIn("Using writable CLI state", self.script)
        self.assertIn("BootstrapRetries", self.script)
        self.assertIn("LASTEXITCODE", self.script)
        self.assertIn("bootstrap command failed with exit code", self.script)
        self.assertIn("--build-path $build", self.script)
        self.assertIn("--flash-size\\s+32MB", self.script)

    def test_workflow_uploads_com4_by_default_and_opens_monitor(self):
        self.assertIn('[string]$Port = "COM4"', self.script)
        self.assertIn("--port $Port", self.script)
        self.assertIn("--input-dir $build", self.script)
        self.assertIn("monitor --port $Port", self.script)
        self.assertIn("System.IO.Ports.SerialPort", self.script)
        self.assertIn("DiscardInBuffer", self.script)
        self.assertIn("finally", self.script)
        self.assertIn("UART tests passed", self.script)
        self.assertIn("SkipTest", self.script)
        self.assertIn("OpenMonitor", self.script)
        self.assertIn("SkipUpload", self.script)
        self.assertIn("SkipMonitor", self.script)

    def test_windows_launcher_bypasses_only_the_process_execution_policy(self):
        launcher = LAUNCHER.read_text(encoding="utf-8")
        self.assertIn("-ExecutionPolicy Bypass", launcher)
        self.assertIn("build_flash_monitor.ps1", launcher)
        self.assertIn("%*", launcher)


if __name__ == "__main__":
    unittest.main()
