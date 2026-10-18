#include "org/minima/utils/encrypt/encrypt_decrypt.hpp"

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/err.h>

#include <algorithm>
#include <memory>
#include <sstream>

namespace org {
namespace minima {
namespace utils {
namespace encrypt {

namespace {

// Helper to get last OpenSSL error as string
[[noreturn]] void throwOpenSSLError(const char* where) {
    unsigned long err = ERR_get_error();
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    std::ostringstream oss;
    oss << where << " failed: OpenSSL error 0x" << std::hex << err << std::dec << " (" << buf << ")";
    throw std::runtime_error(oss.str());
}

struct EVP_PKEY_Deleter {
    void operator()(EVP_PKEY* p) const noexcept { if (p) EVP_PKEY_free(p); }
};
struct EVP_PKEY_CTX_Deleter {
    void operator()(EVP_PKEY_CTX* p) const noexcept { if (p) EVP_PKEY_CTX_free(p); }
};
struct EVP_CIPHER_CTX_Deleter {
    void operator()(EVP_CIPHER_CTX* p) const noexcept { if (p) EVP_CIPHER_CTX_free(p); }
};

using EVP_PKEY_ptr = std::unique_ptr<EVP_PKEY, EVP_PKEY_Deleter>;
using EVP_PKEY_CTX_ptr = std::unique_ptr<EVP_PKEY_CTX, EVP_PKEY_CTX_Deleter>;
using EVP_CIPHER_CTX_ptr = std::unique_ptr<EVP_CIPHER_CTX, EVP_CIPHER_CTX_Deleter>;

EVP_PKEY_ptr parsePublicKey_DER_SPKI(const std::vector<std::uint8_t>& der) {
    const unsigned char* p = der.data();
    EVP_PKEY* raw = d2i_PUBKEY(nullptr, &p, static_cast<long>(der.size()));
    if (!raw) throwOpenSSLError("d2i_PUBKEY");
    return EVP_PKEY_ptr(raw);
}

EVP_PKEY_ptr parsePrivateKey_DER_PKCS8(const std::vector<std::uint8_t>& der) {
    const unsigned char* p = der.data();
    EVP_PKEY* raw = d2i_AutoPrivateKey(nullptr, &p, static_cast<long>(der.size()));
    if (!raw) throwOpenSSLError("d2i_AutoPrivateKey");
    return EVP_PKEY_ptr(raw);
}

const EVP_CIPHER* cipherForAesKeyLenCBC(std::size_t klen) {
    switch (klen) {
        case 16: return EVP_aes_128_cbc();
        case 24: return EVP_aes_192_cbc();
        case 32: return EVP_aes_256_cbc();
        default: return nullptr;
    }
}

} // anonymous

std::vector<std::uint8_t> EncryptDecrypt::encryptASM(const std::vector<std::uint8_t>& zPublicKey,
                                                     const std::vector<std::uint8_t>& inputData) {
    if (zPublicKey.empty()) {
        throw std::runtime_error("encryptASM: empty public key");
    }
    EVP_PKEY_ptr pkey = parsePublicKey_DER_SPKI(zPublicKey);

    EVP_PKEY_CTX_ptr ctx(EVP_PKEY_CTX_new(pkey.get(), nullptr));
    if (!ctx) throwOpenSSLError("EVP_PKEY_CTX_new");

    if (EVP_PKEY_encrypt_init(ctx.get()) <= 0) throwOpenSSLError("EVP_PKEY_encrypt_init");

    // Assume RSA with PKCS#1 v1.5 padding as typical JCE default
    if (EVP_PKEY_base_id(pkey.get()) == EVP_PKEY_RSA) {
        if (EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_PADDING) <= 0) {
            throwOpenSSLError("EVP_PKEY_CTX_set_rsa_padding");
        }
    }

    size_t outlen = 0;
    if (EVP_PKEY_encrypt(ctx.get(), nullptr, &outlen, inputData.data(), inputData.size()) <= 0) {
        throwOpenSSLError("EVP_PKEY_encrypt (size)");
    }

    std::vector<std::uint8_t> out(outlen);
    if (EVP_PKEY_encrypt(ctx.get(), out.data(), &outlen, inputData.data(), inputData.size()) <= 0) {
        throwOpenSSLError("EVP_PKEY_encrypt");
    }
    out.resize(outlen);
    return out;
}

std::vector<std::uint8_t> EncryptDecrypt::decryptASM(const std::vector<std::uint8_t>& zPrivateKey,
                                                     const std::vector<std::uint8_t>& encryptedData) {
    if (zPrivateKey.empty()) {
        throw std::runtime_error("decryptASM: empty private key");
    }
    EVP_PKEY_ptr pkey = parsePrivateKey_DER_PKCS8(zPrivateKey);

    EVP_PKEY_CTX_ptr ctx(EVP_PKEY_CTX_new(pkey.get(), nullptr));
    if (!ctx) throwOpenSSLError("EVP_PKEY_CTX_new");

    if (EVP_PKEY_decrypt_init(ctx.get()) <= 0) throwOpenSSLError("EVP_PKEY_decrypt_init");

    // Assume RSA with PKCS#1 v1.5 padding
    if (EVP_PKEY_base_id(pkey.get()) == EVP_PKEY_RSA) {
        if (EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_PADDING) <= 0) {
            throwOpenSSLError("EVP_PKEY_CTX_set_rsa_padding");
        }
    }

    size_t outlen = 0;
    if (EVP_PKEY_decrypt(ctx.get(), nullptr, &outlen, encryptedData.data(), encryptedData.size()) <= 0) {
        throwOpenSSLError("EVP_PKEY_decrypt (size)");
    }

    std::vector<std::uint8_t> out(outlen);
    if (EVP_PKEY_decrypt(ctx.get(), out.data(), &outlen, encryptedData.data(), encryptedData.size()) <= 0) {
        throwOpenSSLError("EVP_PKEY_decrypt");
    }
    out.resize(outlen);
    return out;
}

std::vector<std::uint8_t> EncryptDecrypt::encryptSYM(const std::vector<std::uint8_t>& zIvParam,
                                                     const std::vector<std::uint8_t>& zSecretKey,
                                                     const std::vector<std::uint8_t>& inputData) {
    const EVP_CIPHER* cipher = cipherForAesKeyLenCBC(zSecretKey.size());
    if (!cipher) {
        throw std::runtime_error("encryptSYM: unsupported AES key length (expected 16/24/32 bytes)");
    }
    const int expected_iv_len = EVP_CIPHER_iv_length(cipher);
    if (static_cast<int>(zIvParam.size()) != expected_iv_len) {
        throw std::runtime_error("encryptSYM: invalid IV length for AES-CBC");
    }

    EVP_CIPHER_CTX_ptr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) throwOpenSSLError("EVP_CIPHER_CTX_new");

    if (EVP_EncryptInit_ex(ctx.get(), cipher, nullptr, zSecretKey.data(), zIvParam.data()) != 1) {
        throwOpenSSLError("EVP_EncryptInit_ex");
    }

    // PKCS#7 padding is enabled by default
    std::vector<std::uint8_t> out(inputData.size() + EVP_CIPHER_block_size(cipher));
    int outlen1 = 0;
    if (EVP_EncryptUpdate(ctx.get(), out.data(), &outlen1, inputData.data(), static_cast<int>(inputData.size())) != 1) {
        throwOpenSSLError("EVP_EncryptUpdate");
    }

    int outlen2 = 0;
    if (EVP_EncryptFinal_ex(ctx.get(), out.data() + outlen1, &outlen2) != 1) {
        throwOpenSSLError("EVP_EncryptFinal_ex");
    }

    out.resize(static_cast<std::size_t>(outlen1 + outlen2));
    return out;
}

std::vector<std::uint8_t> EncryptDecrypt::decryptSYM(const std::vector<std::uint8_t>& zIvParam,
                                                     const std::vector<std::uint8_t>& zSecretKey,
                                                     const std::vector<std::uint8_t>& encryptedData) {
    const EVP_CIPHER* cipher = cipherForAesKeyLenCBC(zSecretKey.size());
    if (!cipher) {
        throw std::runtime_error("decryptSYM: unsupported AES key length (expected 16/24/32 bytes)");
    }
    const int expected_iv_len = EVP_CIPHER_iv_length(cipher);
    if (static_cast<int>(zIvParam.size()) != expected_iv_len) {
        throw std::runtime_error("decryptSYM: invalid IV length for AES-CBC");
    }

    EVP_CIPHER_CTX_ptr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) throwOpenSSLError("EVP_CIPHER_CTX_new");

    if (EVP_DecryptInit_ex(ctx.get(), cipher, nullptr, zSecretKey.data(), zIvParam.data()) != 1) {
        throwOpenSSLError("EVP_DecryptInit_ex");
    }

    std::vector<std::uint8_t> out(encryptedData.size());
    int outlen1 = 0;
    if (EVP_DecryptUpdate(ctx.get(), out.data(), &outlen1, encryptedData.data(), static_cast<int>(encryptedData.size())) != 1) {
        throwOpenSSLError("EVP_DecryptUpdate");
    }

    int outlen2 = 0;
    if (EVP_DecryptFinal_ex(ctx.get(), out.data() + outlen1, &outlen2) != 1) {
        throwOpenSSLError("EVP_DecryptFinal_ex");
    }

    out.resize(static_cast<std::size_t>(outlen1 + outlen2));
    return out;
}

} // namespace encrypt
} // namespace utils
} // namespace minima
} // namespace org