from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SSH = ROOT / "firmware/arduino/esp32shell/ssh_session.h"
TRANSPORT = ROOT / "firmware/arduino/esp32shell/ssh_transport.h"
SUBMODULES = ROOT / ".gitmodules"
DEPENDENCIES = ROOT / "third_party/README.md"


class SshSessionContractTests(unittest.TestCase):
    def test_limits_and_timeout_are_bounded(self):
        source = SSH.read_text(encoding="utf-8")
        self.assertIn("kMaxSessions = 2", source)
        self.assertIn("kMaxLineLength = 4096", source)
        self.assertIn("kIdleTimeoutMs = 300000", source)
        self.assertIn("expire(unsigned long now)", source)

    def test_session_lifecycle_is_explicit(self):
        source = SSH.read_text(encoding="utf-8")
        for method in ("open", "close", "touch", "active"):
            self.assertIn(f"{method}(", source)
        self.assertIn("active_[sessionId]", source)

    def test_transport_fails_closed_without_wolfssh(self):
        source = TRANSPORT.read_text(encoding="utf-8")
        self.assertIn("ESP32SHELL_ENABLE_WOLFSSH", source)
        self.assertIn("disabled-no-wolfssh", source)
        self.assertIn("acceptsPlaintextFallback", source)
        self.assertIn("return false", source)

    def test_official_dependencies_are_pinned(self):
        modules = SUBMODULES.read_text(encoding="utf-8")
        dependencies = DEPENDENCIES.read_text(encoding="utf-8")
        self.assertIn("github.com/wolfSSL/wolfssl.git", modules)
        self.assertIn("github.com/wolfSSL/wolfssh.git", modules)
        self.assertIn("ac01707f552c611fbd135cc723b2682b3e7f80f2", dependencies)
        self.assertIn("8643d7be841184f766374e3b0ed68ced6391543c", dependencies)

    def test_idf_target_builds_real_wolfssh_components(self):
        cmake = (ROOT / "firmware/esp-idf/CMakeLists.txt").read_text(encoding="utf-8")
        main = (ROOT / "firmware/esp-idf/main/ssh_server.cpp").read_text(encoding="utf-8")
        self.assertIn("components", cmake)
        self.assertIn("wolfSSH_Init", main)
        self.assertIn("wolfSSH_Cleanup", main)

    def test_authenticated_upload_is_path_restricted(self):
        services = (ROOT / "firmware/esp-idf/main/idf_command_services.h").read_text(encoding="utf-8")
        self.assertIn('std::strncmp(arguments, "/apps/", 6)', services)
        self.assertIn('std::strstr(arguments, "..")', services)
        self.assertIn('"upload=complete"', services)

    def test_idf_wifi_reads_only_provisioned_nvs_credentials(self):
        source = (ROOT / "firmware/esp-idf/main/wifi_station.cpp").read_text(encoding="utf-8")
        self.assertIn('"wifi_ssid"', source)
        self.assertIn('"wifi_password"', source)
        self.assertIn("esp_wifi_connect", source)
        self.assertIn("Wi-Fi disabled", source)

    def test_idf_mounts_littlefs_without_formatting_on_failure(self):
        source = (ROOT / "firmware/esp-idf/main/littlefs_mount.cpp").read_text(encoding="utf-8")
        manifest = (ROOT / "firmware/esp-idf/main/idf_component.yml").read_text(encoding="utf-8")
        self.assertIn("joltwallet/littlefs", manifest)
        self.assertIn('partition_label = kPartitionLabel', source)
        self.assertIn('format_if_mount_failed = false', source)
        self.assertIn('base_path = kBasePath', source)

    def test_signed_bundle_runtime_requires_signature_and_builtin_descriptor(self):
        source = (ROOT / "firmware/esp-idf/main/idf_app_runtime.cpp").read_text(encoding="utf-8")
        self.assertIn('"app_signing_pub"', source)
        self.assertIn("SignedAppBundle::verify", source)
        self.assertIn('"builtin=led-blink"', source)
        self.assertIn("signed app descriptor is unsupported", source)

    def test_sftp_subsystem_is_enabled_and_rooted_at_littlefs(self):
        cmake = (ROOT / "firmware/esp-idf/components/wolfssh/CMakeLists.txt").read_text(encoding="utf-8")
        source = (ROOT / "firmware/esp-idf/main/ssh_server.cpp").read_text(encoding="utf-8")
        self.assertIn("WOLFSSH_SFTP", cmake)
        self.assertIn("wolfSSH_SFTP_read", source)
        self.assertIn("wolfSSH_worker", source)
        self.assertIn("WS_CHANNEL_CLOSED", source)
        self.assertIn('"/littlefs"', source)
        self.assertIn("WS_SFTP_COMPLETE", source)
        filesystem = (ROOT / "firmware/esp-idf/components/wolfssh/myFilesystem.h").read_text(encoding="utf-8")
        self.assertIn("wolfssh_preinclude.h", cmake)
        self.assertNotIn("WOLFSSH_USER_FILESYSTEM", cmake)
        self.assertIn("USE_OSE_API", (ROOT / "firmware/esp-idf/components/wolfssh/wolfssh_preinclude.h").read_text(encoding="utf-8"))
        self.assertIn("WOPENDIR", filesystem)
        self.assertIn("WPWRITE", filesystem)

    def test_interactive_shell_registers_and_services_channel_request(self):
        source = (ROOT / "firmware/esp-idf/main/ssh_server.cpp").read_text(encoding="utf-8")
        self.assertIn("wolfSSH_CTX_SetChannelReqShellCb(context, shell_request)", source)
        self.assertIn("int shell_request(WOLFSSH_CHANNEL* channel", source)
        self.assertNotIn("wolfSSH_ChannelIsPty", source)
        self.assertIn("const int channelResult = wolfSSH_worker(ssh, nullptr)", source)
        self.assertIn("static volatile bool g_shellRequestAccepted = false", source)
        self.assertIn("g_shellRequestAccepted = true", source)
        self.assertIn("while (!g_shellRequestAccepted)", source)
        self.assertIn("g_shellRequestAccepted = false;\n    int accepted = WS_FATAL_ERROR;", source)
        self.assertIn("for (; handshakeAttempts < 300; ++handshakeAttempts)", source)
        self.assertIn("accepted == WS_AUTH_PENDING", source)
        self.assertIn("error == WS_AUTH_PENDING", source)
        self.assertNotIn("accepted == WS_CHAN_RXD || accepted == WS_AUTH_PENDING", source)
        self.assertNotIn("error == WS_CHAN_RXD || error == WS_AUTH_PENDING", source)
        self.assertIn("vTaskDelay(pdMS_TO_TICKS(10))", source)
        self.assertNotIn("SshOutput output(ssh);\n  g_shellRequestAccepted = false;", source)
        self.assertIn('output.line("esp32shell>")', source)
        self.assertIn("for (int attempt = 0; attempt < 5; ++attempt)", source)
        self.assertIn("SSH shell output failed", source)
        self.assertIn("WS_WINDOW_FULL", source)
        self.assertIn("SSH TCP client accepted", source)
        self.assertIn("SSH handshake result=%d error=%d attempts=%d", source)
        self.assertIn("SSH shell service entered fd=%d", source)
        self.assertIn("SSH shell worker result=%d error=%d request=%d", source)
        self.assertIn("SSH shell request state accepted; sending initial prompt", source)
        self.assertIn("SSH shell output sent line=%u bytes=%u", source)
        self.assertIn("SSH shell read ended result=%d error=%d", source)
        self.assertIn("SSH shell session ended result=%d error=%d", source)
        self.assertIn("SSH shutdown result=%d error=%d", source)

    def test_uart_provisioning_generates_der_key_without_persisting_secrets(self):
        script = (ROOT / "tools/provision_ssh.ps1").read_text(encoding="utf-8")
        self.assertIn("genrsa", script)
        self.assertIn('"-outform", "DER"', script)
        self.assertIn("config-set ssh_host_key", script)
        self.assertIn("Remove-Item -LiteralPath $tempRoot", script)
        self.assertIn("writing RSA key", script)
        self.assertIn("separate strong password", script)
        self.assertIn("Start-Process", script)
        self.assertIn('wifi profile accepted and persisted', script)
        self.assertIn('Read-Host -Prompt $Prompt -AsSecureString', script)
        self.assertIn('if ([string]::IsNullOrWhiteSpace($WifiPassword))', script)
        self.assertIn('if ([string]::IsNullOrWhiteSpace($SshPassword))', script)


if __name__ == "__main__":
    unittest.main()
