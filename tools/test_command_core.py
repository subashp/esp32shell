"""Contract tests for the transport-neutral Phase 1 command core."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "firmware" / "arduino" / "esp32shell" / "command_core.h"
SKETCH = ROOT / "firmware" / "arduino" / "esp32shell" / "esp32shell.ino"


class CommandCoreContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.core = CORE.read_text(encoding="utf-8")
        cls.sketch = SKETCH.read_text(encoding="utf-8")

    def test_all_phase_one_commands_are_registered(self):
        for command in ("help", "version", "device-info", "uptime", "heap", "reboot"):
            self.assertIn(f'"{command}"', self.core)

    def test_result_conventions_cover_empty_unknown_and_invalid_input(self):
        for status in ("Ok", "Empty", "Unknown", "Invalid", "SessionClosed"):
            self.assertIn(status, self.core)
        self.assertIn("error: unknown command; try 'help'", self.core)
        self.assertIn("error: command is too long", self.core)
        self.assertIn("input == nullptr", self.core)

    def test_commands_are_transport_neutral(self):
        self.assertIn("class CommandOutput", self.core)
        self.assertIn("class DeviceServices", self.core)
        self.assertIn("CommandStatus dispatch", self.core)
        self.assertNotIn("Serial", self.core)
        self.assertIn('#include "command_core.h"', self.sketch)
        self.assertIn("commandCore.dispatch", self.sketch)

    def test_serial_input_is_bounded(self):
        self.assertIn("CommandCore::kMaxCommandLength + 1", self.sketch)
        self.assertIn("kMaxCommandLength = 96", self.core)

    def test_exit_and_quit_are_session_commands(self):
        for command in ("exit", "quit"):
            self.assertIn(f'"{command}"', self.core)
        self.assertIn("closeSession", self.core)
        self.assertIn("SessionClosed", self.core)
        self.assertIn("press Ctrl-C to exit", self.sketch)

    def test_argument_commands_provide_usage_without_arguments(self):
        for usage in (
            "usage wifi-config <ssid> <password>",
            "usage config-get <key>",
            "usage config-set <key> <value>",
            "usage config-clear --confirm",
            "usage fs-list <directory>",
            "usage fs-read <path>",
            "usage fs-write <path> <content>",
            "usage fs-remove <path> --confirm",
            "usage gpio-read <allowlisted-pin>",
            "usage gpio-write <allowlisted-pin> <0|1>",
        ):
            self.assertIn(usage, self.core)


if __name__ == "__main__":
    unittest.main()
