#pragma once
#ifndef ORG_MINIMA_UTILS_ENCRYPT_ENCRYPT_DECRYPT_HPP
#define ORG_MINIMA_UTILS_ENCRYPT_ENCRYPT_DECRYPT_HPP

#include <cstdint>
#include <vector>
#include <stdexcept>
#include <string>

namespace org {
namespace minima {
namespace utils {
namespace encrypt {

class EncryptDecrypt {
public:
    // RSA public-key encrypt (Java: encryptASM)
    // zPublicKey: DER-encoded SubjectPublicKeyInfo (X.509) bytes
    // inputData: plaintext
    // returns: RSA-encrypted ciphertext
    static std::vector<std::uint8_t> encryptASM(const std::vector<std::uint8_t>& zPublicKey,
                                                const std::vector<std::uint8_t>& inputData);

    // RSA private-key decrypt (Java: decryptASM)
    // zPrivateKey: DER-encoded PKCS#8 private key bytes
    // encryptedData: RSA ciphertext
    // returns: decrypted plaintext
    static std::vector<std::uint8_t> decryptASM(const std::vector<std::uint8_t>& zPrivateKey,
                                                const std::vector<std::uint8_t>& encryptedData);

    // AES symmetric encrypt (Java: encryptSYM)
    // zIvParam: IV bytes (16 bytes for AES-CBC)
    // zSecretKey: AES key bytes (16/24/32 for 128/192/256-bit)
    // inputData: plaintext
    static std::vector<std::uint8_t> encryptSYM(const std::vector<std::uint8_t>& zIvParam,
                                                const std::vector<std::uint8_t>& zSecretKey,
                                                const std::vector<std::uint8_t>& inputData);

    // AES symmetric decrypt (Java: decryptSYM)
    // zIvParam: IV bytes (16 bytes for AES-CBC)
    // zSecretKey: AES key bytes (16/24/32)
    // encryptedData: ciphertext
    static std::vector<std::uint8_t> decryptSYM(const std::vector<std::uint8_t>& zIvParam,
                                                const std::vector<std::uint8_t>& zSecretKey,
                                                const std::vector<std::uint8_t>& encryptedData);
};

} // namespace encrypt
} // namespace utils
} // namespace minima
} // namespace org

#endif // ORG_MINIMA_UTILS_ENCRYPT_ENCRYPT_DECRYPT_HPP