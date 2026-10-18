#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <exception>

namespace org {
namespace minima {
namespace utils {
namespace encrypt {
namespace javajs {

class AesUtil {
public:
    // keySize in bits (e.g. 128, 192, 256), iterationCount for PBKDF2
    AesUtil(int keySizeBits, int iterationCount);
    ~AesUtil() = default;
    AesUtil(const AesUtil&) = default;
    AesUtil& operator=(const AesUtil&) = default;

    // Returns empty string on failure (Java version returned null)
    std::string decrypt(const std::string& saltHex,
                        const std::string& ivHex,
                        const std::string& passphrase,
                        const std::string& ciphertextBase64);

    // Returns empty string on failure (Java version returned null)
    std::string encrypt(const std::string& saltHex,
                        const std::string& ivHex,
                        const std::string& passphrase,
                        const std::string& plaintext);

    // Static helpers
    static std::vector<std::uint8_t> base64Decode(const std::string& str);
    static std::string base64Encode(const std::vector<std::uint8_t>& data);
    static std::vector<std::uint8_t> hex(const std::string& str);

private:
    int m_keySizeBits;
    int m_iterationCount;

    // encryptMode: true = encrypt, false = decrypt
    std::vector<std::uint8_t> doFinal(bool encryptMode,
                                      const std::vector<std::uint8_t>& key,
                                      const std::string& ivHex,
                                      const std::vector<std::uint8_t>& input);

    std::vector<std::uint8_t> generateKey(const std::string& saltHex,
                                          const std::string& passphrase);

    // Present to mirror Java method; not used in C++ flow
    std::runtime_error fail(const std::exception& e);
};

// Optional demo main translation. Disabled by default.
// Define AESUTIL_BUILD_MAIN to build it.
#ifdef AESUTIL_BUILD_MAIN
int main(int argc, char* argv[]);
#endif

} // namespace javajs
} // namespace encrypt
} // namespace utils
} // namespace minima
} // namespace org