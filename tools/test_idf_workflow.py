"""Source-contract tests for the Windows ESP-IDF SSH workflow."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class IdWorkflowContractTests(unittest.TestCase):
    def test_flash_wrapper_activates_idf_and_preserves_nvs(self):
        script = (ROOT / "tools/flash_espidf_ssh.cmd").read_text(encoding="utf-8")
        self.assertIn("export.bat", script)
        self.assertIn("idf.py -p", script)
        self.assertIn("flash", script)
        self.assertNotIn("erase-flash", script)

    def test_readme_documents_stable_uart_workflow_only(self):
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertIn("build_flash_monitor.cmd", readme)
        self.assertIn("arduino-cli monitor", readme)
        self.assertNotIn("wolfSSH", readme)
        self.assertNotIn("flash_espidf_ssh.cmd", readme)


if __name__ == "__main__":
    unittest.main()
