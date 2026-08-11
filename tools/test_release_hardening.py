from pathlib import Path
import unittest

try:
    from validate_partitions import REQUIRED, validate
except ModuleNotFoundError:  # Direct execution from the repository root.
    from tools.validate_partitions import REQUIRED, validate


ROOT = Path(__file__).resolve().parents[1]
PARTITIONS = ROOT / "firmware/arduino/esp32shell/partitions.csv"
WORKFLOW = ROOT / ".github/workflows/ci.yml"


class ReleaseHardeningTests(unittest.TestCase):
    def test_partition_contract(self):
        self.assertEqual(validate(PARTITIONS), [])
        self.assertEqual(REQUIRED["app0"], (0x10000, 0x800000))
        self.assertEqual(REQUIRED["app1"], (0x810000, 0x800000))

    def test_ci_runs_host_tests_compile_and_partition_validation(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        for required in ("unittest discover", "validate_partitions.py", "arduino-cli compile", "FlashSize=32M,PSRAM=opi"):
            self.assertIn(required, workflow)


if __name__ == "__main__":
    unittest.main()
