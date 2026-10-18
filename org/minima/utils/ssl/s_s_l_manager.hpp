#pragma once

#include <memory>
#include <string>

namespace org {
namespace minima {
namespace utils {
class MinimaLogger;
} // namespace utils
} // namespace minima
} // namespace org

namespace org {
namespace minima {
namespace objects {
namespace base {
class MiniData;
} // namespace base
} // namespace objects
} // namespace minima
} // namespace org

namespace org {
namespace minima {
namespace system {
namespace params {
class GeneralParams;
} // namespace params
} // namespace system
} // namespace minima
} // namespace org

namespace org {
namespace minima {
namespace utils {
namespace ssl {

/**
 * Cross-platform C++ equivalent of org.minima.utils.ssl.SSLManager.
 * Uses OpenSSL to generate and load a PKCS#12 keystore containing a self-signed certificate.
 *
 * Note: The original Java stored the keystore password in MinimaDB's UserDB.
 * The provided C++ headers do not expose the UserDB API; to preserve functional behavior,
 * this C++ version persists the password in a sidecar file next to the keystore file.
 */
class SSLManager {
public:
    // Returns the absolute path to the SSL folder: GeneralParams.DATA_FOLDER/ssl
    static std::string getSSLFolder();

    // Ensures the SSL folder exists, and returns the keystore file path "<folder>/sslkeystore"
    static std::string getKeystoreFile();

    // Ensure keystore and password exist and are valid; create/regenerate if missing or invalid
    static void makeKeyFile();

    // Loaded keystore content (private key, certificate, optional chain) hidden behind PIMPL
    class LoadedKeyStore {
    public:
        ~LoadedKeyStore();
        LoadedKeyStore(LoadedKeyStore&&) noexcept;
        LoadedKeyStore& operator=(LoadedKeyStore&&) noexcept;

        LoadedKeyStore(const LoadedKeyStore&) = delete;
        LoadedKeyStore& operator=(const LoadedKeyStore&) = delete;

        // True if both key and certificate are present
        bool isValid() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;

        LoadedKeyStore(); // Only SSLManager creates instances
        friend class SSLManager;
    };

    // Load and parse the PKCS#12 keystore using the stored password; nullptr on error
    static std::unique_ptr<LoadedKeyStore> getSSLKeyStore();

    // KeyManagerFactory analogue: wraps an OpenSSL SSL_CTX configured for server use
    class KeyManagerFactory {
    public:
        KeyManagerFactory();
        ~KeyManagerFactory();
        KeyManagerFactory(KeyManagerFactory&&) noexcept;
        KeyManagerFactory& operator=(KeyManagerFactory&&) noexcept;

        KeyManagerFactory(const KeyManagerFactory&) = delete;
        KeyManagerFactory& operator=(const KeyManagerFactory&) = delete;

        // Expose native handle as opaque pointer (SSL_CTX*) without requiring OpenSSL headers in this header
        void* nativeHandle() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;

        friend class SSLManager;
    };

    // Create a KeyManagerFactory (SSL_CTX) from a loaded keystore; nullptr on error
    static std::unique_ptr<KeyManagerFactory> getSSLKeyFactory(const LoadedKeyStore& zKeyStore);

private:
    // Generate a new RSA key, self-signed cert, PKCS#12 keystore, and persist them with a random password
    static void generateKeyStore();
};

} // namespace ssl
} // namespace utils
} // namespace minima
} // namespace org