#pragma once

#include <string>
#include <stdexcept>

// OpenSSL for SSL_CTX type parity with Java's SSLContext (BouncyCastle -> OpenSSL)
#include <openssl/ssl.h>

namespace org {
namespace minima {
namespace utils {

class RPCClient {
public:
    // Matches Java: public static String USER_AGENT = "Minima/1.0";
    static std::string USER_AGENT;

    // GET
    static std::string sendGET(const std::string& zHost);
    static std::string sendGETBasicAuth(const std::string& zHost, const std::string& zUser, const std::string& zPassword);
    static std::string sendGETHTTPS(const std::string& zHost);

    // HTTPS with optional SSL context (OpenSSL SSL_CTX*)
    static std::string sendGETSSL(const std::string& zHost);
    static std::string sendGETBasicAuthSSL(const std::string& zHost, const std::string& zUser, const std::string& zPassword, SSL_CTX* zSSLContext);

    // PUT
    static std::string sendPUT(const std::string& zHost);

    // POST
    static std::string sendPOST(const std::string& zHost, const std::string& zParams);
    static std::string sendPOST(const std::string& zHost, const std::string& zParams, const std::string& zType);
    static std::string sendPOSTHTTPS(const std::string& zHost, const std::string& zParams, const std::string& zType);

    // GET with Auth token
    static std::string sendGETAuth(const std::string& zHost, const std::string& zAuthToken);
};

} // namespace utils
} // namespace minima
} // namespace org