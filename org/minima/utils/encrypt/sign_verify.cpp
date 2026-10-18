#include "org/minima/utils/encrypt/sign_verify.hpp"

#include <stdexcept>
#include <sstream>
#include <iostream>

#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/bio.h>

#include "org/minima/objects/base/mini_data.hpp"

namespace org {
namespace minima {
namespace utils {
namespace encrypt {

namespace {

// Custom deleters for OpenSSL pointer types
struct EVP_PKEY_Deleter {
    void operator()(EVP_PKEY* p) const noexcept { if (p) EVP_PKEY_free(p); }
};
struct EVP_MD_CTX_Deleter {
    void operator()(EVP_MD_CTX* p) const noexcept { if (p) EVP_MD_CTX_free(p); }
};
struct PKCS8_PRIV_KEY_INFO_Deleter {
    void operator()(PKCS8_PRIV_KEY_INFO* p) const noexcept { if (p) PKCS8_PRIV_KEY_INFO_free(p); }
};
struct EVP_PKEY_CTX_Deleter {
    void operator()(EVP_PKEY_CTX* p) const noexcept { if (p) EVP_PKEY_CTX_free(p); }
};

// Retrieve last OpenSSL error as string
std::string getOpenSslErrorString() {
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) return "Failed to allocate BIO for OpenSSL error";
    ERR_print_errors(bio);
    char* data = nullptr;
    long len = BIO_get_mem_data(bio, &data);
    std::string err = (len > 0 && data) ? std::string(data, static_cast<size_t>(len)) : std::string();
    BIO_free(bio);
    return err;
}

// Parse a DER-encoded PKCS#8 private key into EVP_PKEY*
std::unique_ptr<EVP_PKEY, EVP_PKEY_Deleter>
loadPrivateKeyFromDer(const std::vector<std::uint8_t>& der) {
    if (der.empty()) {
        throw std::runtime_error("Private key DER is empty");
    }

    // Try PKCS#8 pathway first
    const unsigned char* p = der.data();
    std::unique_ptr<PKCS8_PRIV_KEY_INFO, PKCS8_PRIV_KEY_INFO_Deleter> p8inf(
        d2i_PKCS8_PRIV_KEY_INFO(nullptr, &p, static_cast<long>(der.size()))
    );
    if (p8inf) {
        EVP_PKEY* pk = EVP_PKCS82PKEY(p8inf.get());
        if (pk) {
            return std::unique_ptr<EVP_PKEY, EVP_PKEY_Deleter>(pk);
        }
        // Fallback will try auto
    }

    // Fallback: auto-detect various private key formats
    p = der.data();
    EVP_PKEY* autoPk = d2i_AutoPrivateKey(nullptr, &p, static_cast<long>(der.size()));
    if (autoPk) {
        return std::unique_ptr<EVP_PKEY, EVP_PKEY_Deleter>(autoPk);
    }

    std::ostringstream oss;
    oss << "Failed to parse private key DER. OpenSSL error: " << getOpenSslErrorString();
    throw std::runtime_error(oss.str());
}

// Parse a DER-encoded SubjectPublicKeyInfo into EVP_PKEY*
std::unique_ptr<EVP_PKEY, EVP_PKEY_Deleter>
loadPublicKeyFromDer(const std::vector<std::uint8_t>& der) {
    if (der.empty()) {
        throw std::runtime_error("Public key DER is empty");
    }
    const unsigned char* p = der.data();
    EVP_PKEY* pk = d2i_PUBKEY(nullptr, &p, static_cast<long>(der.size()));
    if (!pk) {
        std::ostringstream oss;
        oss << "Failed to parse public key DER. OpenSSL error: " << getOpenSslErrorString();
        throw std::runtime_error(oss.str());
    }
    return std::unique_ptr<EVP_PKEY, EVP_PKEY_Deleter>(pk);
}

} // anonymous namespace

std::vector<std::uint8_t> SignVerify::sign(const std::vector<std::uint8_t>& privateKeyDer,
                                           const std::vector<std::uint8_t>& message) {
    auto pkey = loadPrivateKeyFromDer(privateKeyDer);

    std::unique_ptr<EVP_MD_CTX, EVP_MD_CTX_Deleter> mdctx(EVP_MD_CTX_new());
    if (!mdctx) {
        throw std::runtime_error("EVP_MD_CTX_new failed");
    }

    if (EVP_DigestSignInit(mdctx.get(), nullptr, EVP_sha256(), nullptr, pkey.get()) != 1) {
        std::ostringstream oss;
        oss << "EVP_DigestSignInit failed. OpenSSL error: " << getOpenSslErrorString();
        throw std::runtime_error(oss.str());
    }

    if (!message.empty()) {
        if (EVP_DigestSignUpdate(mdctx.get(), message.data(), message.size()) != 1) {
            std::ostringstream oss;
            oss << "EVP_DigestSignUpdate failed. OpenSSL error: " << getOpenSslErrorString();
            throw std::runtime_error(oss.str());
        }
    }

    size_t siglen = 0;
    if (EVP_DigestSignFinal(mdctx.get(), nullptr, &siglen) != 1) {
        std::ostringstream oss;
        oss << "EVP_DigestSignFinal (size query) failed. OpenSSL error: " << getOpenSslErrorString();
        throw std::runtime_error(oss.str());
    }

    std::vector<std::uint8_t> signature(siglen);
    if (EVP_DigestSignFinal(mdctx.get(), signature.data(), &siglen) != 1) {
        std::ostringstream oss;
        oss << "EVP_DigestSignFinal failed. OpenSSL error: " << getOpenSslErrorString();
        throw std::runtime_error(oss.str());
    }
    signature.resize(siglen);
    return signature;
}

bool SignVerify::verify(const std::vector<std::uint8_t>& publicKeyDer,
                        const std::vector<std::uint8_t>& message,
                        const std::vector<std::uint8_t>& signature) {
    auto pkey = loadPublicKeyFromDer(publicKeyDer);

    std::unique_ptr<EVP_MD_CTX, EVP_MD_CTX_Deleter> mdctx(EVP_MD_CTX_new());
    if (!mdctx) {
        throw std::runtime_error("EVP_MD_CTX_new failed");
    }

    if (EVP_DigestVerifyInit(mdctx.get(), nullptr, EVP_sha256(), nullptr, pkey.get()) != 1) {
        std::ostringstream oss;
        oss << "EVP_DigestVerifyInit failed. OpenSSL error: " << getOpenSslErrorString();
        throw std::runtime_error(oss.str());
    }

    if (!message.empty()) {
        if (EVP_DigestVerifyUpdate(mdctx.get(), message.data(), message.size()) != 1) {
            std::ostringstream oss;
            oss << "EVP_DigestVerifyUpdate failed. OpenSSL error: " << getOpenSslErrorString();
            throw std::runtime_error(oss.str());
        }
    }

    int vres = EVP_DigestVerifyFinal(mdctx.get(),
                                     signature.data(),
                                     static_cast<size_t>(signature.size()));
    if (vres == 1) {
        return true;
    } else if (vres == 0) {
        return false; // Signature mismatch, mirrors Java's Signature.verify returning false
    } else {
        std::ostringstream oss;
        oss << "EVP_DigestVerifyFinal failed. OpenSSL error: " << getOpenSslErrorString();
        throw std::runtime_error(oss.str());
    }
}

} // namespace encrypt
} // namespace utils
} // namespace minima
} // namespace org
