#include "org/minima/utils/ssl/self_signed_cert_generator.hpp"

#include <stdexcept>
#include <chrono>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <array>
#include <cstdlib> // std::getenv

using namespace org::minima::utils::ssl;

namespace {

// Lowercase helper
std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

// Map Java-style "SHA256withRSA"/"SHA256withECDSA" or "SHA256" -> OpenSSL EVP_MD
const EVP_MD* resolveDigest(const std::string& algo) {
    const std::string a = toLower(algo);
    if (a.find("sha512") != std::string::npos) return EVP_sha512();
    if (a.find("sha384") != std::string::npos) return EVP_sha384();
    if (a.find("sha256") != std::string::npos) return EVP_sha256();
    if (a.find("sha224") != std::string::npos) return EVP_sha224();
    if (a.find("sha1")   != std::string::npos) return EVP_sha1();
    // As a fallback, try to use OpenSSL's name directly (e.g., "sha256")
    if (const EVP_MD* md = EVP_get_digestbyname(a.c_str())) return md;
    return nullptr;
}

// Add an X509v3 extension using OpenSSL's conf-free API
void addExt(X509* cert, int nid, const char* value) {
    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    // Self-signed: issuer == subject
    X509V3_set_ctx(&ctx, cert, cert, nullptr, nullptr, 0);

    X509_EXTENSION* ex = X509V3_EXT_conf_nid(nullptr, &ctx, nid, const_cast<char*>(value));
    if (!ex) {
        throw std::runtime_error("Failed to create X509 extension");
    }
    if (X509_add_ext(cert, ex, -1) != 1) {
        X509_EXTENSION_free(ex);
        throw std::runtime_error("Failed to add X509 extension");
    }
    X509_EXTENSION_free(ex);
}

// Set serial number from uint64 (epoch millis) via BIGNUM to ASN1_INTEGER
void setSerialFromUint64(X509* cert, uint64_t serial) {
    std::array<unsigned char, 8> buf{};
    for (int i = 7; i >= 0; --i) {
        buf[i] = static_cast<unsigned char>(serial & 0xFFu);
        serial >>= 8;
    }

    BIGNUM* bn = BN_bin2bn(buf.data(), static_cast<int>(buf.size()), nullptr);
    if (!bn) {
        throw std::runtime_error("BN_bin2bn failed for serial");
    }

    ASN1_INTEGER* asn1 = X509_get_serialNumber(cert);
    if (!asn1) {
        BN_free(bn);
        throw std::runtime_error("X509_get_serialNumber failed");
    }

    ASN1_INTEGER* res = BN_to_ASN1_INTEGER(bn, asn1);
    BN_free(bn);
    if (!res) {
        throw std::runtime_error("BN_to_ASN1_INTEGER failed");
    }
}

// Get environment variable if defined
std::string getEnv(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

} // anonymous namespace

namespace org {
namespace minima {
namespace utils {
namespace ssl {

const std::string SelfSignedCertGenerator::CERTIFICATE_ALIAS = "MINIMA_NODE";

SelfSignedCertGenerator::X509Ptr
SelfSignedCertGenerator::generate(EVP_PKEY* keyPair,
                                  const std::string& hashAlgorithm,
                                  const std::string& commonName,
                                  int days) {
    if (!keyPair) {
        throw std::runtime_error("generate: keyPair is null");
    }
    if (days <= 0) {
        throw std::runtime_error("generate: days must be > 0");
    }

    // Create new X509 cert
    X509* cert = X509_new();
    if (!cert) {
        throw std::runtime_error("X509_new failed");
    }
    X509Ptr certPtr(cert);

    // Version 3 certificate (value 2)
    if (X509_set_version(cert, 2) != 1) {
        throw std::runtime_error("X509_set_version failed");
    }

    // Serial number = epoch millis
    const auto now = std::chrono::system_clock::now();
    const auto epochMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    if (epochMs < 0) {
        throw std::runtime_error("Negative epoch milliseconds not supported for serial");
    }
    setSerialFromUint64(cert, static_cast<uint64_t>(epochMs));

    // Subject/Issuer Name: CN = commonName
    X509_NAME* name = X509_NAME_new();
    if (!name) {
        throw std::runtime_error("X509_NAME_new failed");
    }
    const unsigned char* cnstr = reinterpret_cast<const unsigned char*>(commonName.c_str());
    if (X509_NAME_add_entry_by_NID(name, NID_commonName, MBSTRING_ASC, cnstr, -1, -1, 0) != 1) {
        X509_NAME_free(name);
        throw std::runtime_error("X509_NAME_add_entry_by_NID failed for CN");
    }
    if (X509_set_subject_name(cert, name) != 1) {
        X509_NAME_free(name);
        throw std::runtime_error("X509_set_subject_name failed");
    }
    if (X509_set_issuer_name(cert, name) != 1) {
        X509_NAME_free(name);
        throw std::runtime_error("X509_set_issuer_name failed");
    }
    X509_NAME_free(name);

    // Validity period: notBefore = now, notAfter = now + days
    if (!X509_gmtime_adj(X509_getm_notBefore(cert), 0)) {
        throw std::runtime_error("X509_gmtime_adj notBefore failed");
    }
    const long seconds = static_cast<long>(static_cast<long long>(days) * 24LL * 60LL * 60LL);
    if (!X509_gmtime_adj(X509_getm_notAfter(cert), seconds)) {
        throw std::runtime_error("X509_gmtime_adj notAfter failed");
    }

    // Public key
    if (X509_set_pubkey(cert, keyPair) != 1) {
        throw std::runtime_error("X509_set_pubkey failed");
    }

    // Extensions:
    // subjectKeyIdentifier = hash
    addExt(cert, NID_subject_key_identifier, "hash");
    // authorityKeyIdentifier = keyid:always
    addExt(cert, NID_authority_key_identifier, "keyid:always");
    // basicConstraints = critical,CA:TRUE
    addExt(cert, NID_basic_constraints, "critical,CA:TRUE");
    // subjectAltName = critical,IP:127.0.0.1,DNS:localhost
    addExt(cert, NID_subject_alt_name, "critical,IP:127.0.0.1,DNS:localhost");

    // Sign
    const EVP_MD* md = resolveDigest(hashAlgorithm);
    if (!md) {
        throw std::runtime_error("Unsupported or unrecognized hash algorithm: " + hashAlgorithm);
    }
    if (X509_sign(cert, keyPair, md) <= 0) {
        throw std::runtime_error("X509_sign failed");
    }

    return certPtr;
}

std::vector<unsigned char>
SelfSignedCertGenerator::createKeystore(X509* cert, EVP_PKEY* key, const std::string& password) {
    if (!cert || !key) {
        throw std::runtime_error("createKeystore: cert or key is null");
    }

    // Create a PKCS#12 (PFX) with given password and alias
    // Use common strong defaults for iterations
    PKCS12* p12 = PKCS12_create(
        password.c_str(),                 // pass
        CERTIFICATE_ALIAS.c_str(),        // friendly name
        key,                              // private key
        cert,                             // certificate
        nullptr,                          // no additional CA certs
        0, // key encryption: 0 -> OpenSSL 3 default (AES-256-CBC + SHA-256)
        0, // cert encryption: 0 -> OpenSSL 3 default (AES-256-CBC + SHA-256)
        2048,                             // key iteration count
        2048,                             // mac iteration count
        0                                 // keytype
    );
    if (!p12) {
        throw std::runtime_error("PKCS12_create failed");
    }

    int len = i2d_PKCS12(p12, nullptr);
    if (len <= 0) {
        PKCS12_free(p12);
        throw std::runtime_error("i2d_PKCS12 (size) failed");
    }

    std::vector<unsigned char> der(static_cast<size_t>(len));
    unsigned char* p = der.data();
    int len2 = i2d_PKCS12(p12, &p);
    PKCS12_free(p12);

    if (len2 != len) {
        throw std::runtime_error("i2d_PKCS12 wrote unexpected length");
    }

    return der;
}

std::vector<unsigned char>
SelfSignedCertGenerator::createKeystore(X509* cert, EVP_PKEY* key) {
    // Attempt to mirror Java behavior of retrieving password from DB.
    // Not possible here due to missing UserDB API; use env var or empty as fallback.
    std::string pass = getEnv("SSL_KEYSTORE_PASS");
    return createKeystore(cert, key, pass);
}

} // namespace ssl
} // namespace utils
} // namespace minima
} // namespace org