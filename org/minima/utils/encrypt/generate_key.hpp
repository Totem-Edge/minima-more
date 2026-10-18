#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>

namespace org {
namespace minima {
namespace utils {
namespace encrypt {

class PublicKey {
public:
    PublicKey();
    explicit PublicKey(EVP_PKEY* pkey); // takes ownership
    PublicKey(const PublicKey& other);
    PublicKey(PublicKey&& other) noexcept;
    PublicKey& operator=(const PublicKey& other);
    PublicKey& operator=(PublicKey&& other) noexcept;
    ~PublicKey();

    bool valid() const;
    EVP_PKEY* raw() const;

private:
    void upref_if_needed();
    EVP_PKEY* m_pkey;
};

class PrivateKey {
public:
    PrivateKey();
    explicit PrivateKey(EVP_PKEY* pkey); // takes ownership
    PrivateKey(const PrivateKey& other);
    PrivateKey(PrivateKey&& other) noexcept;
    PrivateKey& operator=(const PrivateKey& other);
    PrivateKey& operator=(PrivateKey&& other) noexcept;
    ~PrivateKey();

    bool valid() const;
    EVP_PKEY* raw() const;

private:
    void upref_if_needed();
    EVP_PKEY* m_pkey;
};

class SecretKey {
public:
    SecretKey();
    explicit SecretKey(const std::vector<uint8_t>& key);
    explicit SecretKey(std::vector<uint8_t>&& key);

    const std::vector<uint8_t>& bytes() const;

private:
    std::vector<uint8_t> m_key;
};

class Cipher {
public:
    static constexpr int ENCRYPT_MODE = 1;
    static constexpr int DECRYPT_MODE = 2;

    Cipher();
    Cipher(const Cipher&) = delete;
    Cipher& operator=(const Cipher&) = delete;
    Cipher(Cipher&& other) noexcept;
    Cipher& operator=(Cipher&& other) noexcept;
    ~Cipher();

    // RSA init overloads
    void init(int mode, const PublicKey& pubkey);
    void init(int mode, const PrivateKey& privkey);

    // AES init
    void init(int mode, const SecretKey& sk, const std::vector<uint8_t>& iv);

    // One-shot processing similar to Java Cipher#doFinal
    std::vector<uint8_t> doFinal(const std::vector<uint8_t>& input);

private:
    enum class Type { NONE, RSA, AES_CBC };

    void reset();

    Type m_type;
    int m_mode; // ENCRYPT_MODE / DECRYPT_MODE

    // RSA
    EVP_PKEY* m_rsa_key; // retains a reference

    // AES
    EVP_CIPHER_CTX* m_aes_ctx;
    std::vector<uint8_t> m_aes_key;
    std::vector<uint8_t> m_aes_iv;
};

class KeyPair {
public:
    KeyPair();
    KeyPair(const PublicKey& pub, const PrivateKey& priv);

    const PublicKey& getPublic() const;
    const PrivateKey& getPrivate() const;

private:
    PublicKey  m_pub;
    PrivateKey m_priv;
};

class GenerateKey {
public:
    // Algorithm name constants (for parity with Java; informational)
    static constexpr const char* ASYMETRIC_ALGORITHM_GEN = "RSA";
    static constexpr const char* ASYMETRIC_ALGORITHM     = "RSA/ECB/PKCS1Padding";
    static constexpr const char* SYMETRIC_ALGORITHM_GEN  = "AES";
    static constexpr const char* SYMETRIC_ALGORITHM      = "AES/CBC/PKCS5Padding";
    static constexpr const char* SYMETRIC_PASSWORD_ALGORITHM = "PBKDF2WithHmacSHA256";

    // RSA 1024-bit keypair
    static KeyPair generateKeyPair();

    // Convert Java-encoded keys into usable key objects
    // - Public key: X.509 SubjectPublicKeyInfo DER
    // - Private key: PKCS#8 PrivateKeyInfo DER
    static PublicKey convertBytesToPublic(const std::vector<uint8_t>& zPublicKey);
    static PrivateKey convertBytesToPrivate(const std::vector<uint8_t>& zPrivateKey);

    // Generate random AES-128 key raw bytes
    static std::vector<uint8_t> secretKey();

    // Wrap bytes as SecretKey
    static SecretKey convertSecret(const std::vector<uint8_t>& zSecret);

    // PBKDF2WithHmacSHA256(password, salt, 65536 iterations, 128-bit key)
    static SecretKey secretKey(const std::string& zPassword, const std::vector<uint8_t>& zSalt);

    // Random 16-byte IV
    static std::vector<uint8_t> IvParam();

    // Cipher factories
    static Cipher getAsymetricCipher();
    static Cipher getSymetricCipher();

    // Convenience: return initialized AES Cipher
    static Cipher getCipherSYM(int zCipherMode,
                               const std::vector<uint8_t>& zIvParam,
                               const std::vector<uint8_t>& zSecretKey);
};

} // namespace encrypt
} // namespace utils
} // namespace minima
} // namespace org