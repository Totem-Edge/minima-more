#include "org/minima/utils/ssl/minima_trust_manager.hpp"

#include <openssl/x509.h>
#include <openssl/evp.h>

#include "org/minima/objects/base/mini_data.hpp"

namespace org {
namespace minima {
namespace utils {
namespace ssl {

using org::minima::objects::base::MiniData;

MinimaTrustManager::MinimaTrustManager() = default;

MinimaTrustManager::MinimaTrustManager(const MiniData& sslPublicKey)
    : m_sslPublicKey(std::make_unique<MiniData>(sslPublicKey)) {}

// Define special members for unique_ptr to forward-declared type (PIMPL fix)
MinimaTrustManager::~MinimaTrustManager() = default;
MinimaTrustManager::MinimaTrustManager(MinimaTrustManager&&) noexcept = default;
MinimaTrustManager& MinimaTrustManager::operator=(MinimaTrustManager&&) noexcept = default;

std::vector<std::unique_ptr<MinimaTrustManager>> MinimaTrustManager::getTrustManagers() {
    return getTrustManagers(nullptr);
}

std::vector<std::unique_ptr<MinimaTrustManager>> MinimaTrustManager::getTrustManagers(
    const MiniData* sslPublicKey) {
    std::vector<std::unique_ptr<MinimaTrustManager>> managers;
    managers.reserve(1);
    if (sslPublicKey) {
        managers.emplace_back(std::make_unique<MinimaTrustManager>(*sslPublicKey));
    } else {
        managers.emplace_back(std::make_unique<MinimaTrustManager>());
    }
    return managers;
}

bool MinimaTrustManager::hasSSLPublicKey() const noexcept {
    return static_cast<bool>(m_sslPublicKey);
}

std::vector<std::uint8_t> MinimaTrustManager::getPublicKeyDER(X509* cert) {
    std::vector<std::uint8_t> der;
    if (!cert) {
        return der;
    }

    EVP_PKEY* pkey = X509_get_pubkey(cert);
    if (!pkey) {
        return der;
    }

    int len = i2d_PUBKEY(pkey, nullptr);
    if (len <= 0) {
        EVP_PKEY_free(pkey);
        return der;
    }

    der.resize(static_cast<std::size_t>(len));
    unsigned char* outPtr = reinterpret_cast<unsigned char*>(der.data());
    int written = i2d_PUBKEY(pkey, &outPtr);
    EVP_PKEY_free(pkey);

    if (written != len) {
        // In case of mismatch, clear to indicate failure.
        der.clear();
    }

    return der;
}

void MinimaTrustManager::checkTrustedImpl(const std::vector<X509*>& certs, const char* errorMessageIfNotFound) const {
    if (!hasSSLPublicKey()) {
        // No restriction: accept any certificate chain
        return;
    }

    bool found = false;
    for (X509* cert : certs) {
        std::vector<std::uint8_t> der = getPublicKeyDER(cert);
        if (der.empty()) {
            continue;
        }

        MiniData pubk(der);
        if (pubk.isEqual(*m_sslPublicKey)) {
            found = true;
            break;
        }
    }

    if (!found) {
        throw std::runtime_error(errorMessageIfNotFound ? errorMessageIfNotFound : "Invalid SSL Public Key");
    }
}

void MinimaTrustManager::checkClientTrusted(const std::vector<X509*>& certs, const std::string& /*authType*/) const {
    checkTrustedImpl(certs, "Invalid SSL Public Key ( not same as sslpubkey )");
}

void MinimaTrustManager::checkServerTrusted(const std::vector<X509*>& certs, const std::string& /*authType*/) const {
    checkTrustedImpl(certs, "Invalid SSL Public Key");
}

std::vector<X509*> MinimaTrustManager::getAcceptedIssuers() const {
    // Java returns new X509Certificate[0]; we return an empty vector.
    return {};
}

} // namespace ssl
} // namespace utils
} // namespace minima
} // namespace org