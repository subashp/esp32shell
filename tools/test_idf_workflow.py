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

    def test_readme_uses_wrapper_for_unactivated_powershell(self):
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertIn("flash_espidf_ssh.cmd", readme)
        self.assertIn("does not persist into the", readme)


if __name__ == "__main__":
    unittest.main()
