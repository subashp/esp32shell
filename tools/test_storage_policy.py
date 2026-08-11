from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
STORAGE = ROOT / "firmware/arduino/esp32shell/storage_policy.h"


class StoragePolicyContractTests(unittest.TestCase):
    def test_filesystem_paths_are_constrained(self):
        source = STORAGE.read_text(encoding="utf-8")
        self.assertIn("kMaxPathLength = 64", source)
        self.assertIn("path[0] != '/'", source)
        self.assertIn('strstr(path, "..")', source)
        self.assertIn('strstr(path, "//")', source)

    def test_secret_configuration_keys_are_redacted_by_policy(self):
        source = STORAGE.read_text(encoding="utf-8")
        for key in ("wifi_password", "ssh_password", "ssh_private_key"):
            self.assertIn(f'"{key}"', source)


if __name__ == "__main__":
    unittest.main()
