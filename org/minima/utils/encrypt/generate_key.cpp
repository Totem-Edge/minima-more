#include "org/minima/utils/encrypt/generate_key.hpp"

#include <cstring>
#include <sstream>

#ifdef _WIN32
// No special handling required for OpenSSL usage here.
#else
// POSIX systems: no special handling required either.
#endif

namespace org {
namespace minima {
namespace utils {
namespace encrypt {

namespace {

std::string build_openssl_error(const std::string& prefix) {
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) {
        return prefix + ": (unable to allocate BIO for error retrieval)";
    }
    // Drain error queue into BIO
    ERR_print_errors(bio);
    char* data = nullptr;
    long len = BIO_get_mem_data(bio, &data);
    std::string msg = prefix;
    if (len > 0 && data) {
        msg += ": ";
        msg.append(data, static_cast<size_t>(len));
    }
    BIO_free(bio);
    return msg;
}

// Build a public-only EVP_PKEY from a (possibly private) RSA EVP_PKEY.
// We serialize the public key to SubjectPublicKeyInfo DER and reload it.
// This avoids all deprecated low-level RSA APIs and works on both
// OpenSSL 1.1.1 and OpenSSL 3.x. It also strips any private components.
EVP_PKEY* make_public_evp_from_rsa(EVP_PKEY* pkey_src) {
    if (!pkey_src) return nullptr;

    // Export public key to DER (SubjectPublicKeyInfo).
    int len = i2d_PUBKEY(pkey_src, nullptr);
    if (len <= 0) return nullptr;

    std::vector<unsigned char> der(static_cast<std::size_t>(len));
    unsigned char* p = der.data();
    if (i2d_PUBKEY(pkey_src, &p) != len) return nullptr;

    // Reload as a public-only EVP_PKEY.
    const unsigned char* cp = der.data();
    EVP_PKEY* pub = d2i_PUBKEY(nullptr, &cp, static_cast<long>(der.size()));
    return pub;
}

} // anonymous namespace

// KeyPair
KeyPair::KeyPair() : m_pub(), m_priv() {}

KeyPair::KeyPair(const PublicKey& pub, const PrivateKey& priv)
    : m_pub(pub), m_priv(priv) {}

const PublicKey& KeyPair::getPublic() const { return m_pub; }
const PrivateKey& KeyPair::getPrivate() const { return m_priv; }

// PublicKey
PublicKey::PublicKey() : m_pkey(nullptr) {}

PublicKey::PublicKey(EVP_PKEY* pkey) : m_pkey(pkey) {}

PublicKey::PublicKey(const PublicKey& other) : m_pkey(other.m_pkey) {
    upref_if_needed();
}

PublicKey::PublicKey(PublicKey&& other) noexcept : m_pkey(other.m_pkey) {
    other.m_pkey = nullptr;
}

PublicKey& PublicKey::operator=(const PublicKey& other) {
    if (this != &other) {
        if (m_pkey) EVP_PKEY_free(m_pkey);
        m_pkey = other.m_pkey;
        upref_if_needed();
    }
    return *this;
}

PublicKey& PublicKey::operator=(PublicKey&& other) noexcept {
    if (this != &other) {
        if (m_pkey) EVP_PKEY_free(m_pkey);
        m_pkey = other.m_pkey;
        other.m_pkey = nullptr;
    }
    return *this;
}

PublicKey::~PublicKey() {
    if (m_pkey) EVP_PKEY_free(m_pkey);
}

bool PublicKey::valid() const { return m_pkey != nullptr; }
EVP_PKEY* PublicKey::raw() const { return m_pkey; }
void PublicKey::upref_if_needed() { if (m_pkey) EVP_PKEY_up_ref(m_pkey); }

// PrivateKey
PrivateKey::PrivateKey() : m_pkey(nullptr) {}

PrivateKey::PrivateKey(EVP_PKEY* pkey) : m_pkey(pkey) {}

PrivateKey::PrivateKey(const PrivateKey& other) : m_pkey(other.m_pkey) {
    upref_if_needed();
}

PrivateKey::PrivateKey(PrivateKey&& other) noexcept : m_pkey(other.m_pkey) {
    other.m_pkey = nullptr;
}

PrivateKey& PrivateKey::operator=(const PrivateKey& other) {
    if (this != &other) {
        if (m_pkey) EVP_PKEY_free(m_pkey);
        m_pkey = other.m_pkey;
        upref_if_needed();
    }
    return *this;
}

PrivateKey& PrivateKey::operator=(PrivateKey&& other) noexcept {
    if (this != &other) {
        if (m_pkey) EVP_PKEY_free(m_pkey);
        m_pkey = other.m_pkey;
        other.m_pkey = nullptr;
    }
    return *this;
}

PrivateKey::~PrivateKey() {
    if (m_pkey) EVP_PKEY_free(m_pkey);
}

bool PrivateKey::valid() const { return m_pkey != nullptr; }
EVP_PKEY* PrivateKey::raw() const { return m_pkey; }
void PrivateKey::upref_if_needed() { if (m_pkey) EVP_PKEY_up_ref(m_pkey); }

// SecretKey
SecretKey::SecretKey() : m_key() {}
SecretKey::SecretKey(const std::vector<uint8_t>& key) : m_key(key) {}
SecretKey::SecretKey(std::vector<uint8_t>&& key) : m_key(std::move(key)) {}
const std::vector<uint8_t>& SecretKey::bytes() const { return m_key; }

// Cipher
Cipher::Cipher()
    : m_type(Type::NONE),
      m_mode(0),
      m_rsa_key(nullptr),
      m_aes_ctx(nullptr),
      m_aes_key(),
      m_aes_iv() {
}

Cipher::Cipher(Cipher&& other) noexcept
    : m_type(other.m_type),
      m_mode(other.m_mode),
      m_rsa_key(other.m_rsa_key),
      m_aes_ctx(other.m_aes_ctx),
      m_aes_key(std::move(other.m_aes_key)),
      m_aes_iv(std::move(other.m_aes_iv)) {
    other.m_type = Type::NONE;
    other.m_mode = 0;
    other.m_rsa_key = nullptr;
    other.m_aes_ctx = nullptr;
}

Cipher& Cipher::operator=(Cipher&& other) noexcept {
    if (this != &other) {
        reset();
        m_type = other.m_type;
        m_mode = other.m_mode;
        m_rsa_key = other.m_rsa_key;
        m_aes_ctx = other.m_aes_ctx;
        m_aes_key = std::move(other.m_aes_key);
        m_aes_iv = std::move(other.m_aes_iv);

        other.m_type = Type::NONE;
        other.m_mode = 0;
        other.m_rsa_key = nullptr;
        other.m_aes_ctx = nullptr;
    }
    return *this;
}

Cipher::~Cipher() {
    reset();
}

void Cipher::reset() {
    if (m_aes_ctx) {
        EVP_CIPHER_CTX_free(m_aes_ctx);
        m_aes_ctx = nullptr;
    }
    if (m_rsa_key) {
        // We hold a reference; release it.
        EVP_PKEY_free(m_rsa_key);
        m_rsa_key = nullptr;
    }
    m_type = Type::NONE;
    m_mode = 0;
    m_aes_key.clear();
    m_aes_iv.clear();
}

void Cipher::init(int mode, const PublicKey& pubkey) {
    reset();
    if (mode != ENCRYPT_MODE && mode != DECRYPT_MODE) {
        throw std::runtime_error("Cipher.init: invalid mode for RSA");
    }
    if (!pubkey.valid()) {
        throw std::runtime_error("Cipher.init: invalid public key");
    }
    m_type = Type::RSA;
    m_mode = mode;
    m_rsa_key = pubkey.raw();
    EVP_PKEY_up_ref(m_rsa_key);
}

void Cipher::init(int mode, const PrivateKey& privkey) {
    reset();
    if (mode != ENCRYPT_MODE && mode != DECRYPT_MODE) {
        throw std::runtime_error("Cipher.init: invalid mode for RSA");
    }
    if (!privkey.valid()) {
        throw std::runtime_error("Cipher.init: invalid private key");
    }
    m_type = Type::RSA;
    m_mode = mode;
    m_rsa_key = privkey.raw();
    EVP_PKEY_up_ref(m_rsa_key);
}

void Cipher::init(int mode, const SecretKey& sk, const std::vector<uint8_t>& iv) {
    reset();
    if (mode != ENCRYPT_MODE && mode != DECRYPT_MODE) {
        throw std::runtime_error("Cipher.init: invalid mode for AES");
    }
    if (iv.size() != 16) {
        throw std::runtime_error("Cipher.init: AES-CBC requires 16-byte IV");
    }
    const auto& key = sk.bytes();
    if (key.size() != 16) {
        throw std::runtime_error("Cipher.init: AES-128 requires 16-byte key");
    }
    m_type = Type::AES_CBC;
    m_mode = mode;
    m_aes_key = key;
    m_aes_iv = iv;
    m_aes_ctx = EVP_CIPHER_CTX_new();
    if (!m_aes_ctx) {
        throw std::runtime_error(build_openssl_error("EVP_CIPHER_CTX_new failed"));
    }

    const EVP_CIPHER* cipher = EVP_aes_128_cbc();
    int ok = 0;
    if (mode == ENCRYPT_MODE) {
        ok = EVP_EncryptInit_ex(m_aes_ctx, cipher, nullptr, m_aes_key.data(), m_aes_iv.data());
    } else {
        ok = EVP_DecryptInit_ex(m_aes_ctx, cipher, nullptr, m_aes_key.data(), m_aes_iv.data());
    }
    if (ok != 1) {
        throw std::runtime_error(build_openssl_error("EVP_*Init_ex failed"));
    }
    // PKCS padding is enabled by default; matches Java "PKCS5Padding".
}

std::vector<uint8_t> Cipher::doFinal(const std::vector<uint8_t>& input) {
    if (m_type == Type::RSA) {
        if (!m_rsa_key) {
            throw std::runtime_error("Cipher.doFinal: RSA key not initialized");
        }
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(m_rsa_key, nullptr);
        if (!ctx) {
            throw std::runtime_error(build_openssl_error("EVP_PKEY_CTX_new failed"));
        }

        std::vector<uint8_t> output;
        size_t outlen = 0;
        int ok = 0;

        if (m_mode == ENCRYPT_MODE) {
            ok = EVP_PKEY_encrypt_init(ctx);
            if (ok != 1) {
                EVP_PKEY_CTX_free(ctx);
                throw std::runtime_error(build_openssl_error("EVP_PKEY_encrypt_init failed"));
            }
            ok = EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING);
            if (ok <= 0) {
                EVP_PKEY_CTX_free(ctx);
                throw std::runtime_error(build_openssl_error("EVP_PKEY_CTX_set_rsa_padding failed"));
            }
            ok = EVP_PKEY_encrypt(ctx, nullptr, &outlen, input.data(), input.size());
            if (ok != 1) {
                EVP_PKEY_CTX_free(ctx);
                throw std::runtime_error(build_openssl_error("EVP_PKEY_encrypt (size query) failed"));
            }
            output.resize(outlen);
            ok = EVP_PKEY_encrypt(ctx, output.data(), &outlen, input.data(), input.size());
            if (ok != 1) {
                EVP_PKEY_CTX_free(ctx);
                throw std::runtime_error(build_openssl_error("EVP_PKEY_encrypt failed"));
            }
            output.resize(outlen);
        } else if (m_mode == DECRYPT_MODE) {
            ok = EVP_PKEY_decrypt_init(ctx);
            if (ok != 1) {
                EVP_PKEY_CTX_free(ctx);
                throw std::runtime_error(build_openssl_error("EVP_PKEY_decrypt_init failed"));
            }
            ok = EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING);
            if (ok <= 0) {
                EVP_PKEY_CTX_free(ctx);
                throw std::runtime_error(build_openssl_error("EVP_PKEY_CTX_set_rsa_padding failed"));
            }
            ok = EVP_PKEY_decrypt(ctx, nullptr, &outlen, input.data(), input.size());
            if (ok != 1) {
                EVP_PKEY_CTX_free(ctx);
                throw std::runtime_error(build_openssl_error("EVP_PKEY_decrypt (size query) failed"));
            }
            output.resize(outlen);
            ok = EVP_PKEY_decrypt(ctx, output.data(), &outlen, input.data(), input.size());
            if (ok != 1) {
                EVP_PKEY_CTX_free(ctx);
                throw std::runtime_error(build_openssl_error("EVP_PKEY_decrypt failed"));
            }
            output.resize(outlen);
        } else {
            EVP_PKEY_CTX_free(ctx);
            throw std::runtime_error("Cipher.doFinal: invalid RSA mode");
        }

        EVP_PKEY_CTX_free(ctx);
        return output;
    } else if (m_type == Type::AES_CBC) {
        if (!m_aes_ctx) {
            throw std::runtime_error("Cipher.doFinal: AES context not initialized");
        }

        std::vector<uint8_t> output;
        int outlen1 = static_cast<int>(input.size() + EVP_CIPHER_block_size(EVP_aes_128_cbc()));
        output.resize(outlen1);

        int produced = 0;
        int ok = 0;

        if (m_mode == ENCRYPT_MODE) {
            // Re-init to start a fresh operation with same key/iv
            ok = EVP_EncryptInit_ex(m_aes_ctx, nullptr, nullptr, nullptr, nullptr);
            if (ok != 1) {
                throw std::runtime_error(build_openssl_error("EVP_EncryptInit_ex (re-init) failed"));
            }
            ok = EVP_EncryptUpdate(m_aes_ctx, output.data(), &produced, input.data(),
                                   static_cast<int>(input.size()));
            if (ok != 1) {
                throw std::runtime_error(build_openssl_error("EVP_EncryptUpdate failed"));
            }
            int total = produced;
            int finalProduced = 0;
            ok = EVP_EncryptFinal_ex(m_aes_ctx, output.data() + total, &finalProduced);
            if (ok != 1) {
                throw std::runtime_error(build_openssl_error("EVP_EncryptFinal_ex failed"));
            }
            total += finalProduced;
            output.resize(total);
        } else if (m_mode == DECRYPT_MODE) {
            ok = EVP_DecryptInit_ex(m_aes_ctx, nullptr, nullptr, nullptr, nullptr);
            if (ok != 1) {
                throw std::runtime_error(build_openssl_error("EVP_DecryptInit_ex (re-init) failed"));
            }
            ok = EVP_DecryptUpdate(m_aes_ctx, output.data(), &produced, input.data(),
                                   static_cast<int>(input.size()));
            if (ok != 1) {
                throw std::runtime_error(build_openssl_error("EVP_DecryptUpdate failed"));
            }
            int total = produced;
            int finalProduced = 0;
            ok = EVP_DecryptFinal_ex(m_aes_ctx, output.data() + total, &finalProduced);
            if (ok != 1) {
                throw std::runtime_error(build_openssl_error("EVP_DecryptFinal_ex failed (bad padding or wrong key/iv?)"));
            }
            total += finalProduced;
            output.resize(total);
        } else {
            throw std::runtime_error("Cipher.doFinal: invalid AES mode");
        }
        return output;
    } else {
        throw std::runtime_error("Cipher.doFinal: cipher not initialized");
    }
}

// GenerateKey
KeyPair GenerateKey::generateKeyPair() {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!ctx) {
        throw std::runtime_error(build_openssl_error("EVP_PKEY_CTX_new_id failed"));
    }

    if (EVP_PKEY_keygen_init(ctx) != 1) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error(build_openssl_error("EVP_PKEY_keygen_init failed"));
    }

    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 1024) != 1) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error(build_openssl_error("EVP_PKEY_CTX_set_rsa_keygen_bits failed"));
    }

    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen(ctx, &pkey) != 1) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error(build_openssl_error("EVP_PKEY_keygen failed"));
    }
    EVP_PKEY_CTX_free(ctx);

    // Create distinct public and private wrappers
    EVP_PKEY* pub = make_public_evp_from_rsa(pkey);
    if (!pub) {
        EVP_PKEY_free(pkey);
        throw std::runtime_error(build_openssl_error("Failed to extract public key from generated RSA keypair"));
    }

    PublicKey pk_pub(pub);      // takes ownership
    PrivateKey pk_priv(pkey);   // takes ownership

    return KeyPair(pk_pub, pk_priv);
}

PublicKey GenerateKey::convertBytesToPublic(const std::vector<uint8_t>& zPublicKey) {
    const unsigned char* p = zPublicKey.data();
    EVP_PKEY* pkey = d2i_PUBKEY(nullptr, &p, static_cast<long>(zPublicKey.size()));
    if (!pkey) {
        throw std::runtime_error(build_openssl_error("d2i_PUBKEY failed (expecting X.509 SubjectPublicKeyInfo DER)"));
    }
    return PublicKey(pkey); // takes ownership
}

PrivateKey GenerateKey::convertBytesToPrivate(const std::vector<uint8_t>& zPrivateKey) {
    const unsigned char* p = zPrivateKey.data();
    PKCS8_PRIV_KEY_INFO* p8inf = d2i_PKCS8_PRIV_KEY_INFO(nullptr, &p, static_cast<long>(zPrivateKey.size()));
    if (!p8inf) {
        throw std::runtime_error(build_openssl_error("d2i_PKCS8_PRIV_KEY_INFO failed (expecting PKCS#8 DER)"));
    }
    EVP_PKEY* pkey = EVP_PKCS82PKEY(p8inf);
    PKCS8_PRIV_KEY_INFO_free(p8inf);
    if (!pkey) {
        throw std::runtime_error(build_openssl_error("EVP_PKCS82PKEY failed to convert PKCS#8 to EVP_PKEY"));
    }
    return PrivateKey(pkey); // takes ownership
}

std::vector<uint8_t> GenerateKey::secretKey() {
    std::vector<uint8_t> key(16);
    if (RAND_bytes(key.data(), static_cast<int>(key.size())) != 1) {
        throw std::runtime_error(build_openssl_error("RAND_bytes failed for AES key"));
    }
    return key;
}

SecretKey GenerateKey::convertSecret(const std::vector<uint8_t>& zSecret) {
    return SecretKey(zSecret);
}

SecretKey GenerateKey::secretKey(const std::string& zPassword, const std::vector<uint8_t>& zSalt) {
    std::vector<uint8_t> key(16);
    const int iterations = 65536;
    if (PKCS5_PBKDF2_HMAC(zPassword.c_str(),
                          static_cast<int>(zPassword.size()),
                          zSalt.data(), static_cast<int>(zSalt.size()),
                          iterations, EVP_sha256(),
                          static_cast<int>(key.size()),
                          key.data()) != 1) {
        throw std::runtime_error(build_openssl_error("PKCS5_PBKDF2_HMAC failed"));
    }
    return SecretKey(std::move(key));
}

std::vector<uint8_t> GenerateKey::IvParam() {
    std::vector<uint8_t> iv(16);
    if (RAND_bytes(iv.data(), static_cast<int>(iv.size())) != 1) {
        throw std::runtime_error(build_openssl_error("RAND_bytes failed for IV"));
    }
    return iv;
}

Cipher GenerateKey::getAsymetricCipher() {
    Cipher c;
    // Not initialized yet; type will be set during init().
    return c;
}

Cipher GenerateKey::getSymetricCipher() {
    Cipher c;
    // Not initialized yet; type will be set during init().
    return c;
}

Cipher GenerateKey::getCipherSYM(int zCipherMode,
                                 const std::vector<uint8_t>& zIvParam,
                                 const std::vector<uint8_t>& zSecretKey) {
    SecretKey sk = convertSecret(zSecretKey);
    Cipher c = getSymetricCipher();
    c.init(zCipherMode, sk, zIvParam);
    return c;
}

} // namespace encrypt
} // namespace utils
} // namespace minima
} // namespace org