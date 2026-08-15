from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "firmware/esp-idf/main/idf_log_buffer.cpp"
SERVICES = ROOT / "firmware/esp-idf/main/idf_command_services.h"
CORE = ROOT / "firmware/arduino/esp32shell/command_core.h"


class EspIdfLogBufferContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text(encoding="utf-8")
        cls.services = SERVICES.read_text(encoding="utf-8")
        cls.core = CORE.read_text(encoding="utf-8")

    def test_ring_is_bounded_and_overwrites_oldest(self):
        self.assertIn("kMaxEntries = 64", self.source)
        self.assertIn("kMaxMessageLength = 128", self.source)
        self.assertIn("nextEntry = (nextEntry + 1) % kMaxEntries", self.source)

    def test_logs_are_tee_and_shell_retrieval_is_explicit(self):
        self.assertIn("esp_log_set_vprintf", self.source)
        self.assertIn("previousVprintf(format, arguments)", self.source)
        self.assertIn("void log_dump", self.source)
        self.assertIn("log_dump(output)", self.services)
        self.assertIn('"dmesg"', self.core)


if __name__ == "__main__":
    unittest.main()
