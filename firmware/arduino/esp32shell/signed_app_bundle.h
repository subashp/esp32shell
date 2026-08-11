#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace esp32shell {

// App bundles are data-driven tasks, not native code blobs.  This keeps an
// uploaded LED/task description from becoming an arbitrary-code loader while
// still allowing a signed bundle to select a built-in app implementation.
struct SignedAppBundleHeader {
  uint8_t magic[8];       // "ESAPP01\0"
  uint8_t formatVersion;  // currently 1
  uint8_t nameLength;
  uint16_t reserved;
  uint32_t payloadLength;
  uint8_t payloadSha256[32];
  uint8_t signature[64];  // Ed25519 signature over header-without-signature + payload
};

class AppBundleDigest {
 public:
  virtual ~AppBundleDigest() = default;
  virtual bool sha256(const uint8_t* data, size_t length,
                      uint8_t digest[32]) = 0;
};

class AppBundleSignatureVerifier {
 public:
  virtual ~AppBundleSignatureVerifier() = default;
  virtual bool verify(const uint8_t digest[32], const uint8_t signature[64]) = 0;
};

class SignedAppBundle {
 public:
  static constexpr uint8_t kMagic[8] = {'E', 'S', 'A', 'P', 'P', '0', '1', 0};
  static constexpr size_t kMaxNameLength = 31;
  static constexpr size_t kMaxPayloadLength = 64 * 1024;

  static bool verify(const SignedAppBundleHeader& header, const char* name,
                     const uint8_t* payload, size_t payloadLength,
                     AppBundleDigest& digest,
                     AppBundleSignatureVerifier& signatureVerifier) {
    if (memcmp(header.magic, kMagic, sizeof(kMagic)) != 0 ||
        header.formatVersion != 1 || name == nullptr || payload == nullptr ||
        header.nameLength == 0 || header.nameLength > kMaxNameLength ||
        strlen(name) != header.nameLength || payloadLength == 0 ||
        payloadLength > kMaxPayloadLength || header.payloadLength != payloadLength) {
      return false;
    }
    uint8_t payloadDigest[32] = {};
    if (!digest.sha256(payload, payloadLength, payloadDigest) ||
        memcmp(payloadDigest, header.payloadSha256, sizeof(payloadDigest)) != 0) {
      return false;
    }
    uint8_t signedDigest[32] = {};
    if (!digest.sha256(payload, payloadLength, signedDigest)) return false;
    return signatureVerifier.verify(signedDigest, header.signature);
  }
};

}  // namespace esp32shell
