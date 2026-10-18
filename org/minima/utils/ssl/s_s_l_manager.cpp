#include "org/minima/utils/ssl/s_s_l_manager.hpp"

#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <chrono>
#include <filesystem>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pkcs12.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include "org/minima/system/params/general_params.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/minima_logger.hpp"

// Helper: OpenSSL error stack as string
static std::string opensslLastError() {
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) return "BIO_new failed";
    ERR_print_errors(bio);
    char* data = nullptr;
    long len = BIO_get_mem_data(bio, &data);
    std::string out = (data && len > 0) ? std::string(data, static_cast<size_t>(len)) : std::string("Unknown OpenSSL error");
    BIO_free(bio);
    return out;
}

namespace fs = std::filesystem;

namespace org {
namespace minima {
namespace utils {
namespace ssl {

// =================== LoadedKeyStore PIMPL ===================
struct SSLManager::LoadedKeyStore::Impl {
    EVP_PKEY* pkey   = nullptr;
    X509*     cert   = nullptr;
    STACK_OF(X509)* chain = nullptr;
    std::string password;

    ~Impl() {
        if (chain) {
            while (sk_X509_num(chain) > 0) {
                X509* xc = sk_X509_pop(chain);
                if (xc) X509_free(xc);
            }
            sk_X509_free(chain);
        }
        if (cert) X509_free(cert);
        if (pkey) EVP_PKEY_free(pkey);
    }

    bool valid() const { return pkey != nullptr && cert != nullptr; }
};

SSLManager::LoadedKeyStore::LoadedKeyStore() : m_impl(std::make_unique<Impl>()) {}
SSLManager::LoadedKeyStore::~LoadedKeyStore() = default;
SSLManager::LoadedKeyStore::LoadedKeyStore(LoadedKeyStore&&) noexcept = default;
SSLManager::LoadedKeyStore& SSLManager::LoadedKeyStore::operator=(LoadedKeyStore&&) noexcept = default;
bool SSLManager::LoadedKeyStore::isValid() const { return m_impl && m_impl->valid(); }

// =================== KeyManagerFactory PIMPL ===================
struct SSLManager::KeyManagerFactory::Impl {
    SSL_CTX* ctx = nullptr;
    ~Impl() {
        if (ctx) {
            SSL_CTX_free(ctx);
            ctx = nullptr;
        }
    }
};

SSLManager::KeyManagerFactory::KeyManagerFactory() : m_impl(std::make_unique<Impl>()) {}
SSLManager::KeyManagerFactory::~KeyManagerFactory() = default;
SSLManager::KeyManagerFactory::KeyManagerFactory(KeyManagerFactory&&) noexcept = default;
SSLManager::KeyManagerFactory& SSLManager::KeyManagerFactory::operator=(KeyManagerFactory&&) noexcept = default;
void* SSLManager::KeyManagerFactory::nativeHandle() const { return m_impl ? static_cast<void*>(m_impl->ctx) : nullptr; }

// =================== Internal helpers ===================
static std::string getKeystorePassFilePath(const std::string& keystorePath) {
    return keystorePath + ".pass";
}

static bool fileExists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

static bool ensureDir(const std::string& dir) {
    std::error_code ec;
    if (fs::exists(dir, ec)) {
        return fs::is_directory(dir, ec);
    }
    return fs::create_directories(dir, ec);
}

static std::string trim(const std::string& s) {
    auto b = std::find_if_not(s.begin(), s.end(), [](unsigned char c){ return std::isspace(c); });
    auto e = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c){ return std::isspace(c); }).base();
    if (b >= e) return "";
    return std::string(b, e);
}

static bool writeTextFile(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(out);
}

static bool readTextFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

static int add_ext(X509* cert, int nid, const char* value, X509* issuer, X509* subject) {
    if (!cert || !value) return 0;

    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, issuer, subject, nullptr, nullptr, 0);

    X509_EXTENSION* ex = X509V3_EXT_conf_nid(nullptr, &ctx, nid, const_cast<char*>(value));
    if (!ex) return 0;

    int ret = X509_add_ext(cert, ex, -1);
    X509_EXTENSION_free(ex);
    return ret;
}

static bool generate_rsa_key_and_self_signed_cert(EVP_PKEY** out_pkey, X509** out_cert) {
    if (!out_pkey || !out_cert) return false;

    bool ok = false;
    EVP_PKEY* pkey = nullptr;
    X509* cert = nullptr;
    ASN1_INTEGER* serial = nullptr;
    X509_NAME* name = nullptr;
    EVP_PKEY_CTX* kgctx = nullptr;
    std::uint64_t s = 0;

    // Modern EVP key generation: no deprecated RSA_* APIs.
    kgctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!kgctx) goto end;
    if (EVP_PKEY_keygen_init(kgctx) != 1) goto end;
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(kgctx, 4096) != 1) goto end;
    if (EVP_PKEY_keygen(kgctx, &pkey) != 1) goto end;

    cert = X509_new();
    if (!cert) goto end;

    // Version: X509v3 (value 2)
    if (X509_set_version(cert, 2) != 1) goto end;

    // Serial number
    serial = ASN1_INTEGER_new();
    if (!serial) goto end;
    s = static_cast<std::uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    if (s == 0) s = 1;
    if (!ASN1_INTEGER_set_uint64(serial, s)) goto end;
    if (X509_set_serialNumber(cert, serial) != 1) goto end;
    ASN1_INTEGER_free(serial);
    serial = nullptr;

    // Subject and Issuer: CN=localhost
    name = X509_NAME_new();
    if (!name) goto end;
    if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                   reinterpret_cast<const unsigned char*>("localhost"),
                                   -1, -1, 0) != 1) {
        goto end;
    }
    if (X509_set_issuer_name(cert, name) != 1) goto end;
    if (X509_set_subject_name(cert, name) != 1) goto end;
    X509_NAME_free(name);
    name = nullptr;

    // Validity: now - 1 day to now + 730 days
    if (!X509_gmtime_adj(X509_getm_notBefore(cert), -60L*60L*24L)) goto end;
    if (!X509_gmtime_adj(X509_getm_notAfter(cert), 60L*60L*24L*730L)) goto end;

    // Public key
    if (X509_set_pubkey(cert, pkey) != 1) goto end;

    // Extensions
    if (!add_ext(cert, NID_basic_constraints, "CA:FALSE", cert, cert)) goto end;
    if (!add_ext(cert, NID_key_usage, "digitalSignature,keyEncipherment", cert, cert)) goto end;
    if (!add_ext(cert, NID_ext_key_usage, "serverAuth", cert, cert)) goto end;
    if (!add_ext(cert, NID_subject_alt_name, "DNS:localhost", cert, cert)) goto end;

    // Sign with SHA256
    if (X509_sign(cert, pkey, EVP_sha256()) <= 0) goto end;

    ok = true;

end:
    if (name) { X509_NAME_free(name); name = nullptr; }
    if (serial) { ASN1_INTEGER_free(serial); serial = nullptr; }
    if (kgctx) { EVP_PKEY_CTX_free(kgctx); kgctx = nullptr; }

    if (ok) {
        *out_pkey = pkey;
        *out_cert = cert;
        pkey = nullptr;
        cert = nullptr;
    }

    if (cert) X509_free(cert);
    if (pkey) EVP_PKEY_free(pkey);

    return ok;
}

// =================== SSLManager public methods ===================

std::string SSLManager::getSSLFolder() {
    fs::path base = org::minima::system::params::GeneralParams::DATA_FOLDER;
    fs::path ssldir = base / "ssl";
    return ssldir.string();
}

std::string SSLManager::getKeystoreFile() {
    std::string folder = getSSLFolder();
    if (!ensureDir(folder)) {
        org::minima::utils::MinimaLogger::log("Failed to create SSL folder: " + folder);
    }
    fs::path kspath = fs::path(folder) / "sslkeystore";
    return kspath.string();
}

void SSLManager::makeKeyFile() {
    try {
        const std::string ksfile   = getKeystoreFile();
        const std::string passfile = getKeystorePassFilePath(ksfile);

        bool needGenerate = false;

        if (!fileExists(ksfile)) {
            needGenerate = true;
        }

        std::string keystorepass;
        if (!fileExists(passfile) || !readTextFile(passfile, keystorepass)) {
            needGenerate = true;
        } else {
            keystorepass = trim(keystorepass);
            if (keystorepass.empty()) {
                needGenerate = true;
            }
        }

        if (needGenerate) {
            generateKeyStore();
        } else {
            org::minima::utils::MinimaLogger::log("Loading SSL Keystore.. ");
            auto ks = getSSLKeyStore();
            if (!ks || !ks->isValid()) {
                org::minima::utils::MinimaLogger::log("Issue with keystore.. regenerate.");
                generateKeyStore();
            }
        }
    } catch (const std::exception& exc) {
        org::minima::utils::MinimaLogger::log(exc);
    }
}

void SSLManager::generateKeyStore() {
    // Log type as PKCS12 (modern Java default)
    org::minima::utils::MinimaLogger::log("Generating SSL Keystore.. PKCS12");

    // Random password as in Java: MiniData.getRandomData(32).to0xString()
    std::string keystorepass = org::minima::objects::base::MiniData::getRandomData(32).to0xString();

    const std::string ksfile   = getKeystoreFile();
    const std::string passfile = getKeystorePassFilePath(ksfile);

    // Generate key and self-signed certificate
    EVP_PKEY* pkey = nullptr;
    X509* cert = nullptr;
    if (!generate_rsa_key_and_self_signed_cert(&pkey, &cert)) {
        std::string err = "Failed generating key/cert: " + opensslLastError();
        throw std::runtime_error(err);
    }

    // Create PKCS#12
    PKCS12* p12 = PKCS12_create(
        keystorepass.c_str(),           // password
        "minima",                       // friendlyName
        pkey,                           // private key
        cert,                           // certificate
        nullptr,                        // ca chain
        0, 0,                           // default key/cert encryption
        2048,                           // MAC iteration
        2048,                           // key iteration
        0                               // keytype
    );
    if (!p12) {
        EVP_PKEY_free(pkey);
        X509_free(cert);
        std::string err = "Failed creating PKCS12: " + opensslLastError();
        throw std::runtime_error(err);
    }

    // Write PKCS#12 to file
    BIO* out = BIO_new_file(ksfile.c_str(), "wb");
    if (!out) {
        PKCS12_free(p12);
        EVP_PKEY_free(pkey);
        X509_free(cert);
        std::string err = "Failed opening keystore file for write: " + ksfile;
        throw std::runtime_error(err);
    }
    int derok = i2d_PKCS12_bio(out, p12);
    BIO_free(out);
    PKCS12_free(p12);
    EVP_PKEY_free(pkey);
    X509_free(cert);
    if (derok != 1) {
        std::string err = "Failed writing PKCS12: " + opensslLastError();
        throw std::runtime_error(err);
    }

    // Persist the password locally (sidecar), mirroring Java's persistence intent
    if (!writeTextFile(passfile, keystorepass)) {
        throw std::runtime_error("Failed to write keystore password file: " + passfile);
    }
}

std::unique_ptr<SSLManager::LoadedKeyStore> SSLManager::getSSLKeyStore() {
    try {
        const std::string ksfile   = getKeystoreFile();
        const std::string passfile = getKeystorePassFilePath(ksfile);

        std::string keystorepass;
        if (!readTextFile(passfile, keystorepass)) {
            org::minima::utils::MinimaLogger::log("getSSLKeyStore: Unable to read password file.");
            return nullptr;
        }
        keystorepass = trim(keystorepass);
        if (keystorepass.empty()) {
            org::minima::utils::MinimaLogger::log("getSSLKeyStore: Empty keystore password.");
            return nullptr;
        }

        BIO* in = BIO_new_file(ksfile.c_str(), "rb");
        if (!in) {
            org::minima::utils::MinimaLogger::log("getSSLKeyStore: Unable to open keystore file: " + ksfile);
            return nullptr;
        }

        PKCS12* p12 = d2i_PKCS12_bio(in, nullptr);
        BIO_free(in);
        if (!p12) {
            org::minima::utils::MinimaLogger::log("getSSLKeyStore: PKCS12 parse error (read): " + opensslLastError());
            return nullptr;
        }

        EVP_PKEY* pkey = nullptr;
        X509* cert = nullptr;
        STACK_OF(X509)* ca = nullptr;

        if (PKCS12_parse(p12, keystorepass.c_str(), &pkey, &cert, &ca) != 1) {
            PKCS12_free(p12);
            org::minima::utils::MinimaLogger::log("getSSLKeyStore: PKCS12 parse error: " + opensslLastError());
            return nullptr;
        }

        PKCS12_free(p12);

        auto loaded = std::unique_ptr<LoadedKeyStore>(new LoadedKeyStore());
        loaded->m_impl->pkey = pkey;
        loaded->m_impl->cert = cert;
        loaded->m_impl->chain = ca;
        loaded->m_impl->password = keystorepass;

        return loaded;
    } catch (const std::exception& exc) {
        org::minima::utils::MinimaLogger::log(exc);
        return nullptr;
    }
}

std::unique_ptr<SSLManager::KeyManagerFactory> SSLManager::getSSLKeyFactory(const LoadedKeyStore& zKeyStore) {
    try {
        if (!zKeyStore.isValid()) {
            org::minima::utils::MinimaLogger::log("getSSLKeyFactory: Invalid keystore.");
            return nullptr;
        }

        const EVP_PKEY* pkey = zKeyStore.m_impl->pkey;
        const X509* cert     = zKeyStore.m_impl->cert;
        STACK_OF(X509)* chain = zKeyStore.m_impl->chain;

        // Create SSL_CTX for server
        const SSL_METHOD* method = TLS_server_method();
        if (!method) {
            org::minima::utils::MinimaLogger::log("getSSLKeyFactory: TLS_server_method failed: " + opensslLastError());
            return nullptr;
        }

        auto kmf = std::unique_ptr<KeyManagerFactory>(new KeyManagerFactory());
        kmf->m_impl->ctx = SSL_CTX_new(method);
        if (!kmf->m_impl->ctx) {
            org::minima::utils::MinimaLogger::log("getSSLKeyFactory: SSL_CTX_new failed: " + opensslLastError());
            return nullptr;
        }

        SSL_CTX* ctx = kmf->m_impl->ctx;

        // Load certificate and key into context
        if (SSL_CTX_use_certificate(ctx, const_cast<X509*>(cert)) != 1) {
            org::minima::utils::MinimaLogger::log("getSSLKeyFactory: use_certificate failed: " + opensslLastError());
            return nullptr;
        }
        if (SSL_CTX_use_PrivateKey(ctx, const_cast<EVP_PKEY*>(pkey)) != 1) {
            org::minima::utils::MinimaLogger::log("getSSLKeyFactory: use_PrivateKey failed: " + opensslLastError());
            return nullptr;
        }
        if (SSL_CTX_check_private_key(ctx) != 1) {
            org::minima::utils::MinimaLogger::log("getSSLKeyFactory: check_private_key failed: " + opensslLastError());
            return nullptr;
        }

        // If we have a chain, add it
        if (chain && sk_X509_num(chain) > 0) {
            for (int i = 0; i < sk_X509_num(chain); ++i) {
                X509* cacert = sk_X509_value(chain, i);
                X509* dup = X509_dup(cacert);
                if (!dup) {
                    org::minima::utils::MinimaLogger::log("getSSLKeyFactory: X509_dup failed: " + opensslLastError());
                    return nullptr;
                }
                if (SSL_CTX_add_extra_chain_cert(ctx, dup) != 1) {
                    X509_free(dup);
                    org::minima::utils::MinimaLogger::log("getSSLKeyFactory: add_extra_chain_cert failed: " + opensslLastError());
                    return nullptr;
                }
            }
        }

        return kmf;
    } catch (const std::exception& exc) {
        org::minima::utils::MinimaLogger::log(exc);
        return nullptr;
    }
}

} // namespace ssl
} // namespace utils
} // namespace minima
} // namespace org