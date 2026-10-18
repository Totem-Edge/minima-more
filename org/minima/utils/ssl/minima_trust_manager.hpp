#pragma once

#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdint>

namespace org { namespace minima { namespace objects { namespace base { class MiniData; }}}}

// Forward-declare OpenSSL X509 type to avoid heavy headers in the header file
typedef struct x509_st X509;

namespace org {
namespace minima {
namespace utils {
namespace ssl {

class MinimaTrustManager {
public:
    MinimaTrustManager();
    explicit MinimaTrustManager(const org::minima::objects::base::MiniData& sslPublicKey);

    // Explicit special members due to unique_ptr to forward-declared type
    virtual ~MinimaTrustManager();
    MinimaTrustManager(MinimaTrustManager&&) noexcept;
    MinimaTrustManager& operator=(MinimaTrustManager&&) noexcept;

    MinimaTrustManager(const MinimaTrustManager&) = delete;
    MinimaTrustManager& operator=(const MinimaTrustManager&) = delete;

    // Factory methods matching Java semantics: return a single trust manager
    static std::vector<std::unique_ptr<MinimaTrustManager>> getTrustManagers();
    static std::vector<std::unique_ptr<MinimaTrustManager>> getTrustManagers(
        const org::minima::objects::base::MiniData* sslPublicKey);

    // Throws std::runtime_error on failure (maps from CertificateException)
    void checkClientTrusted(const std::vector<X509*>& certs, const std::string& authType) const;
    void checkServerTrusted(const std::vector<X509*>& certs, const std::string& authType) const;

    // Returns empty set, matching Java's "new X509Certificate[0]"
    std::vector<X509*> getAcceptedIssuers() const;

private:
    std::unique_ptr<org::minima::objects::base::MiniData> m_sslPublicKey;

    bool hasSSLPublicKey() const noexcept;
    void checkTrustedImpl(const std::vector<X509*>& certs, const char* errorMessageIfNotFound) const;
    static std::vector<std::uint8_t> getPublicKeyDER(X509* cert);
};

} // namespace ssl
} // namespace utils
} // namespace minima
} // namespace org