from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SESSION = ROOT / "firmware/common/shell_session.h"
TRANSPORT = ROOT / "firmware/common/shell_transport.h"
SSH = ROOT / "firmware/esp-idf/main/ssh_server.cpp"


class ShellSessionRefactorTests(unittest.TestCase):
    def test_common_transport_is_byte_oriented(self):
        source = TRANSPORT.read_text(encoding="utf-8")
        self.assertIn("class ShellTransport", source)
        self.assertIn("ShellReadStatus", source)
        self.assertIn("virtual bool write(const char* data, size_t length)", source)
        self.assertIn("virtual bool connected() const", source)
        self.assertNotIn("wolfSSH", source)

    def test_session_owns_terminal_behavior(self):
        source = SESSION.read_text(encoding="utf-8")
        for fragment in (
            "class ShellSession",
            "CommandCore::kMaxCommandLength + 1",
            'output_.raw("\\r\\n")',
            'output_.raw("\\b \\b")',
            "previousWasCarriageReturn",
            "CommandStatus::SessionClosed",
            'output_.prompt()',
        ):
            self.assertIn(fragment, source)

    def test_ssh_server_adapts_transport_without_line_editing(self):
        source = SSH.read_text(encoding="utf-8")
        self.assertIn('#include "../../../common/shell_session.h"', source)
        self.assertIn("class SshTransport final : public esp32shell::ShellTransport", source)
        self.assertIn("esp32shell::ShellSession session(transport, core, services)", source)
        self.assertNotIn("previousWasCarriageReturn", source)
        self.assertNotIn("char echoed[2]", source)
        self.assertNotIn("CommandCore::kMaxCommandLength + 1]", source)

    def test_idf_does_not_start_a_uart_shell(self):
        source = SSH.read_text(encoding="utf-8")
        self.assertNotIn("UartShellTransport", source)
        self.assertNotIn("uart-shell", source)


if __name__ == "__main__":
    unittest.main()
