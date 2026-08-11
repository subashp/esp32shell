from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class AppBundleContractTests(unittest.TestCase):
    def test_bundle_is_data_driven_and_bounded(self):
        source = (ROOT / "firmware/arduino/esp32shell/signed_app_bundle.h").read_text(encoding="utf-8")
        self.assertIn('"ESAPP01', source)
        self.assertIn("kMaxPayloadLength = 64 * 1024", source)
        self.assertIn("AppBundleSignatureVerifier", source)
        self.assertIn("payloadSha256", source)

    def test_bundle_rejects_bad_digest_or_signature(self):
        source = (ROOT / "firmware/arduino/esp32shell/signed_app_bundle.h").read_text(encoding="utf-8")
        self.assertIn("memcmp(payloadDigest, header.payloadSha256", source)
        self.assertIn("signatureVerifier.verify", source)


if __name__ == "__main__":
    unittest.main()
