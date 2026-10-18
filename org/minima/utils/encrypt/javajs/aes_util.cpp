#include "org/minima/utils/encrypt/javajs/aes_util.hpp"

#include <stdexcept>
#include <cstring>
#include <iostream>

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/err.h>

#include "org/minima/objects/base/mini_data.hpp"

namespace org {
namespace minima {
namespace utils {
namespace encrypt {
namespace javajs {

AesUtil::AesUtil(int keySizeBits, int iterationCount)
    : m_keySizeBits(keySizeBits), m_iterationCount(iterationCount) {
    // Java tried to instantiate a Cipher and would throw via fail(e) if unavailable.
    // In OpenSSL, cipher availability is handled at use-time in doFinal.
}

std::vector<std::uint8_t> AesUtil::generateKey(const std::string& saltHex,
                                               const std::string& passphrase) {
    try {
        std::vector<std::uint8_t> salt = hex(saltHex);
        if (salt.empty()) {
            return {};
        }
        if (m_keySizeBits % 8 != 0) {
            return {};
        }
        const int keyLen = m_keySizeBits / 8;
        std::vector<std::uint8_t> key(static_cast<size_t>(keyLen));

        // PBKDF2WithHmacSHA1
        int ok = PKCS5_PBKDF2_HMAC_SHA1(
            passphrase.data(),
            static_cast<int>(passphrase.size()),
            salt.data(),
            static_cast<int>(salt.size()),
            m_iterationCount,
            keyLen,
            key.data()
        );
        if (ok != 1) {
            return {};
        }
        return key;
    } catch (...) {
        return {};
    }
}

static const EVP_CIPHER* selectAesCbcCipher(int keyBits) {
    switch (keyBits) {
        case 128: return EVP_aes_128_cbc();
        case 192: return EVP_aes_192_cbc();
        case 256: return EVP_aes_256_cbc();
        default:  return nullptr;
    }
}

std::vector<std::uint8_t> AesUtil::doFinal(bool encryptMode,
                                           const std::vector<std::uint8_t>& key,
                                           const std::string& ivHex,
                                           const std::vector<std::uint8_t>& input) {
    try {
        const EVP_CIPHER* cipher = selectAesCbcCipher(m_keySizeBits);
        if (!cipher) {
            return {};
        }
        std::vector<std::uint8_t> iv = hex(ivHex);
        // AES CBC requires 16-byte IV
        if (iv.size() != 16) {
            return {};
        }

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            return {};
        }

        std::vector<std::uint8_t> output;
        int outlen1 = 0, outlen2 = 0;
        const int blockSize = EVP_CIPHER_block_size(cipher);
        output.resize(input.size() + static_cast<size_t>(blockSize));

        int ok = 0;
        if (encryptMode) {
            ok = EVP_EncryptInit_ex(ctx, cipher, nullptr, key.data(), iv.data());
            if (ok != 1) { EVP_CIPHER_CTX_free(ctx); return {}; }

            ok = EVP_EncryptUpdate(ctx, output.data(), &outlen1, input.data(), static_cast<int>(input.size()));
            if (ok != 1) { EVP_CIPHER_CTX_free(ctx); return {}; }

            ok = EVP_EncryptFinal_ex(ctx, output.data() + outlen1, &outlen2);
            if (ok != 1) { EVP_CIPHER_CTX_free(ctx); return {}; }
        } else {
            ok = EVP_DecryptInit_ex(ctx, cipher, nullptr, key.data(), iv.data());
            if (ok != 1) { EVP_CIPHER_CTX_free(ctx); return {}; }

            ok = EVP_DecryptUpdate(ctx, output.data(), &outlen1, input.data(), static_cast<int>(input.size()));
            if (ok != 1) { EVP_CIPHER_CTX_free(ctx); return {}; }

            ok = EVP_DecryptFinal_ex(ctx, output.data() + outlen1, &outlen2);
            if (ok != 1) {
                // Mirrors Java catching BadPaddingException / IllegalBlockSizeException → return null
                EVP_CIPHER_CTX_free(ctx);
                return {};
            }
        }

        output.resize(static_cast<size_t>(outlen1 + outlen2));
        EVP_CIPHER_CTX_free(ctx);
        return output;
    } catch (...) {
        return {};
    }
}

std::string AesUtil::decrypt(const std::string& saltHex,
                             const std::string& ivHex,
                             const std::string& passphrase,
                             const std::string& ciphertextBase64) {
    try {
        std::vector<std::uint8_t> key = generateKey(saltHex, passphrase);
        if (key.empty()) {
            return std::string();
        }
        std::vector<std::uint8_t> cipherbytes = base64Decode(ciphertextBase64);
        if (cipherbytes.empty() && !ciphertextBase64.empty()) {
            // decode failed
            return std::string();
        }
        std::vector<std::uint8_t> plain = doFinal(false, key, ivHex, cipherbytes);
        if (plain.empty()) {
            return std::string();
        }
        return std::string(reinterpret_cast<const char*>(plain.data()), plain.size());
    } catch (...) {
        return std::string();
    }
}

std::string AesUtil::encrypt(const std::string& saltHex,
                             const std::string& ivHex,
                             const std::string& passphrase,
                             const std::string& plaintext) {
    try {
        std::vector<std::uint8_t> key = generateKey(saltHex, passphrase);
        if (key.empty()) {
            return std::string();
        }
        std::vector<std::uint8_t> plainbytes(plaintext.begin(), plaintext.end());
        std::vector<std::uint8_t> enc = doFinal(true, key, ivHex, plainbytes);
        if (enc.empty()) {
            return std::string();
        }
        return base64Encode(enc);
    } catch (...) {
        return std::string();
    }
}

std::vector<std::uint8_t> AesUtil::base64Decode(const std::string& str) {
    try {
        // Remove whitespace to mimic Java Base64 decoder tolerance
        std::string in;
        in.reserve(str.size());
        for (char c : str) {
            if (c != '\n' && c != '\r' && c != '\t' && c != ' ') in.push_back(c);
        }
        if (in.empty()) {
            return {};
        }
        if (in.size() % 4 != 0) {
            return {};
        }

        const size_t outCap = 3 * (in.size() / 4);
        std::vector<std::uint8_t> out(outCap);
        int decoded = EVP_DecodeBlock(out.data(),
                                      reinterpret_cast<const unsigned char*>(in.data()),
                                      static_cast<int>(in.size()));
        if (decoded < 0) {
            return {};
        }

        // Adjust for padding '=' chars
        size_t pad = 0;
        if (!in.empty() && in[in.size() - 1] == '=') pad++;
        if (in.size() >= 2 && in[in.size() - 2] == '=') pad++;
        size_t realLen = static_cast<size_t>(decoded) - pad;
        if (realLen > out.size()) {
            return {};
        }
        out.resize(realLen);
        return out;
    } catch (...) {
        return {};
    }
}

std::string AesUtil::base64Encode(const std::vector<std::uint8_t>& data) {
    if (data.empty()) {
        return std::string();
    }
    const size_t outLen = 4 * ((data.size() + 2) / 3);
    std::string out;
    out.resize(outLen);
    int written = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&out[0]),
                                  data.data(),
                                  static_cast<int>(data.size()));
    if (written < 0) {
        return std::string();
    }
    if (static_cast<size_t>(written) != outLen) {
        out.resize(static_cast<size_t>(written));
    }
    return out;
}

std::vector<std::uint8_t> AesUtil::hex(const std::string& str) {
    try {
        org::minima::objects::base::MiniData md(str);
        return md.getBytes();
    } catch (const std::exception& e) {
        // Mirrors Java throwing IllegalStateException which is then caught at higher level
        throw std::runtime_error(std::string("hex decode failed: ") + e.what());
    } catch (...) {
        throw std::runtime_error("hex decode failed: unknown error");
    }
}

std::runtime_error AesUtil::fail(const std::exception& e) {
    // Java returned null and then 'throw fail(e)' caused a NullPointerException.
    // In C++, return a runtime_error; caller may throw it.
    return std::runtime_error(std::string("AesUtil init failed: ") + e.what());
}

#ifdef AESUTIL_BUILD_MAIN
int main(int /*argc*/, char* /*argv*/[]) {
    std::string text = "helloyou!";
    std::string password = "apasswordblabla";

    AesUtil aesUtil(128, 1000);

    std::string cipher = aesUtil.encrypt("4452150bad3b58e9d2c043ad24db2b1d",
                                         "0d93cefd0147ecb48a379b5be0da7e8a",
                                         password,
                                         text);

    std::cout << cipher << std::endl;

    std::string plain = aesUtil.decrypt("4452150bad3b58e9d2c043ad24db2b1d",
                                        "0d93cefd0147ecb48a379b5be0da7e8a",
                                        password,
                                        cipher);

    std::cout << plain << std::endl;

    return 0;
}
#endif

} // namespace javajs
} // namespace encrypt
} // namespace utils
} // namespace minima
} // namespace org