#pragma once

#include <string>
#include <vector>
#include <memory>

// OpenSSL headers (external dependency mapped from BouncyCastle)
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/evp.h>
#include <openssl/pkcs12.h>

namespace org {
namespace minima {
namespace utils {
namespace ssl {

class SelfSignedCertGenerator final {
public:
    struct X509Deleter {
        void operator()(X509* p) const noexcept { if (p) X509_free(p); }
    };
    using X509Ptr = std::unique_ptr<X509, X509Deleter>;

    // Java: public static final String CERTIFICATE_ALIAS = "MINIMA_NODE";
    static const std::string CERTIFICATE_ALIAS;

    // Java: generate(KeyPair keyPair, String hashAlgorithm, String cn, int days) -> X509Certificate
    // C++: keyPair -> OpenSSL EVP_PKEY* containing both private and public key
    // Returns a self-signed X.509 certificate with the same extensions as Java version.
    static X509Ptr generate(EVP_PKEY* keyPair,
                            const std::string& hashAlgorithm,
                            const std::string& commonName,
                            int days);

    // Java: createKeystore(X509Certificate cert, PrivateKey key) -> KeyStore
    // C++: produce a PKCS#12 (PFX) keystore (DER-encoded bytes) containing cert+key.
    // This overload uses environment variable SSL_KEYSTORE_PASS (if set) or empty password.
    static std::vector<unsigned char> createKeystore(X509* cert, EVP_PKEY* key);

    // Overload allowing explicit password.
    static std::vector<unsigned char> createKeystore(X509* cert, EVP_PKEY* key, const std::string& password);
};

} // namespace ssl
} // namespace utils
} // namespace minima
} // namespace org