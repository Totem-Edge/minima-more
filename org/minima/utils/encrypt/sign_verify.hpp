#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace org {
namespace minima {
namespace utils {
namespace encrypt {

class SignVerify {
public:
    // Algorithm name preserved for parity with Java constant
    inline static constexpr const char* SIGN_ALGO = "SHA256withRSA";

    // Sign the given message with a DER-encoded PKCS#8 RSA private key.
    // Throws std::runtime_error on errors parsing the key or performing the signature.
    static std::vector<std::uint8_t> sign(const std::vector<std::uint8_t>& privateKeyDer,
                                          const std::vector<std::uint8_t>& message);

    // Verify the given signature using a DER-encoded X.509 SubjectPublicKeyInfo RSA public key.
    // Returns true if the signature is valid; false if it is not.
    // Throws std::runtime_error on errors parsing the key or initializing verification.
    static bool verify(const std::vector<std::uint8_t>& publicKeyDer,
                       const std::vector<std::uint8_t>& message,
                       const std::vector<std::uint8_t>& signature);
};

} // namespace encrypt
} // namespace utils
} // namespace minima
} // namespace org