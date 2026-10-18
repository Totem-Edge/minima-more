#include "org/minima/utils/r_p_c_client.hpp"

#include <curl/curl.h>
#include <vector>
#include <iostream>
#include <sstream>
#include <memory>
#include <algorithm>

namespace org {
namespace minima {
namespace utils {

std::string RPCClient::USER_AGENT = "Minima/1.0";

// RAII initializer for libcurl (thread-safe static initialization)
namespace {
struct CurlGlobalInit {
    CurlGlobalInit() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobalInit() { curl_global_cleanup(); }
};

// Curl write callback accumulates response body
const size_t MAX_PEER_LIST_DOWNLOAD = 1 * 1024 * 1024;
static size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto realSize = size * nmemb;
    if (userdata && realSize > 0) {
        std::string* out = static_cast<std::string*>(userdata);
        if (out->length() + realSize > MAX_PEER_LIST_DOWNLOAD) {
            // Data is too large, stop the download
            return 0; // Returning 0 aborts the curl download
        }
        out->append(ptr, realSize);
    }
    return realSize;
}

static bool starts_with_https(const std::string& url) {
    // Java check: zHost.startsWith("https")
    if (url.size() < 5) return false;
    std::string prefix = url.substr(0, 5);
    std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::tolower);
    return prefix == "https";
}

// Simple Base64 encoder (for Basic Auth header)
static std::string base64_encode(const std::string& in) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i + 3 <= in.size()) {
        unsigned int b = (static_cast<unsigned char>(in[i]) << 16) |
                         (static_cast<unsigned char>(in[i + 1]) << 8) |
                         (static_cast<unsigned char>(in[i + 2]));
        out.push_back(table[(b >> 18) & 0x3F]);
        out.push_back(table[(b >> 12) & 0x3F]);
        out.push_back(table[(b >> 6) & 0x3F]);
        out.push_back(table[b & 0x3F]);
        i += 3;
    }

    if (i < in.size()) {
        unsigned int b = static_cast<unsigned char>(in[i]) << 16;
        out.push_back(table[(b >> 18) & 0x3F]);
        if (i + 1 < in.size()) {
            b |= static_cast<unsigned char>(in[i + 1]) << 8;
            out.push_back(table[(b >> 12) & 0x3F]);
            out.push_back(table[(b >> 6) & 0x3F]);
            out.push_back('=');
        } else {
            out.push_back(table[(b >> 12) & 0x3F]);
            out.push_back('=');
            out.push_back('=');
        }
    }

    return out;
}

struct CurlDeleter {
    void operator()(curl_slist* p) const { if (p) curl_slist_free_all(p); }
};

// Helper to perform an HTTP request and return body and response code
static std::pair<std::string, long> performRequest(
    const std::string& url,
    const std::string& method,                                   // "GET", "POST", "PUT"
    const std::vector<std::string>& headers,                     // extra headers
    const std::string* body,                                     // nullptr if no body
    bool followRedirects,
    long connectTimeoutMs
) {
    // throw std::runtime_error("CURL is disabled for testing");
    static CurlGlobalInit curlInit;

    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response;
    long http_code = 0;
    std::unique_ptr<curl_slist, CurlDeleter> hdrs(nullptr);

    // Build headers list
    // Always add "Connection: close" per Java behavior
    hdrs.reset(curl_slist_append(hdrs.release(), "Connection: close"));
    for (const auto& h : headers) {
        hdrs.reset(curl_slist_append(hdrs.release(), h.c_str()));
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, RPCClient::USER_AGENT.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs.get());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, followRedirects ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); // avoid signals, safer in multi-threaded contexts
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connectTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    if (method == "GET") {
        // Default is GET
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (body) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body->data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body->size()));
        } else {
            // Empty body
            const char* empty = "";
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, empty);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
        }
    } else if (method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        // Java sendPUT does not write a request body, so none here.
    } else {
        curl_easy_cleanup(curl);
        throw std::invalid_argument("Unsupported HTTP method: " + method);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        // Get HTTP code if available
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        std::string err = curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        throw std::runtime_error("CURL perform failed: " + err);
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    return {response, http_code};
}

static void print_http_not_ok(const std::string& method, long code, const std::string& host) {
    // Matches Java prints:
    // "GET request not HTTP_OK resp:"+responseCode+" @ "+zHost
    // "PUT request not HTTP_OK resp:"+responseCode+" @ "+zHost
    std::cout << method << " request not HTTP_OK resp:" << code << " @ " << host << std::endl;
}

} // anonymous namespace

// ========== Public static methods ==========

std::string RPCClient::sendGET(const std::string& zHost) {
    if (starts_with_https(zHost)) {
        return sendGETHTTPS(zHost);
    } else {
        return sendGETBasicAuth(zHost, "", "");
    }
}

std::string RPCClient::sendGETBasicAuth(const std::string& zHost, const std::string& zUser, const std::string& zPassword) {
    std::vector<std::string> headers;
    if (!zPassword.empty()) {
        std::string userpass = zUser + ":" + zPassword;
        std::string basicAuth = "Authorization: Basic " + base64_encode(userpass);
        headers.push_back(basicAuth);
    }

    auto result = performRequest(zHost, "GET", headers, nullptr, false, 10000);
    const long code = result.second;
    if (code == 200) {
        return result.first;
    } else {
        print_http_not_ok("GET", code, zHost);
        return "";
    }
}

std::string RPCClient::sendGETHTTPS(const std::string& zHost) {
    std::vector<std::string> headers; // No extra headers
    auto result = performRequest(zHost, "GET", headers, nullptr, false, 10000);
    const long code = result.second;
    if (code == 200) {
        return result.first;
    } else {
        print_http_not_ok("GET", code, zHost);
        return "";
    }
}

std::string RPCClient::sendGETSSL(const std::string& zHost) {
    return sendGETBasicAuthSSL(zHost, "", "", nullptr);
}

std::string RPCClient::sendGETBasicAuthSSL(const std::string& zHost, const std::string& zUser, const std::string& zPassword, SSL_CTX* /*zSSLContext*/) {
    // Note: zSSLContext is currently not applied to libcurl's SSL context.
    // If custom trust/pinning is required, integrate via CURLOPT_SSL_CTX_FUNCTION/CURLOPT_CAINFO/CURLOPT_PINNEDPUBLICKEY.
    std::vector<std::string> headers;
    if (!zPassword.empty()) {
        std::string userpass = zUser + ":" + zPassword;
        std::string basicAuth = "Authorization: Basic " + base64_encode(userpass);
        headers.push_back(basicAuth);
    }

    auto result = performRequest(zHost, "GET", headers, nullptr, false, 10000);
    const long code = result.second;
    if (code == 200) {
        return result.first;
    } else {
        print_http_not_ok("GET", code, zHost);
        return "";
    }
}

std::string RPCClient::sendPUT(const std::string& zHost) {
    std::vector<std::string> headers; // No extra headers beyond Connection/User-Agent
    auto result = performRequest(zHost, "PUT", headers, nullptr, false, 10000);
    const long code = result.second;
    if (code == 200) {
        return result.first;
    } else {
        print_http_not_ok("PUT", code, zHost);
        return "";
    }
}

std::string RPCClient::sendPOST(const std::string& zHost, const std::string& zParams) {
    if (starts_with_https(zHost)) {
        return sendPOSTHTTPS(zHost, zParams, nullptr);
    } else {
        return sendPOST(zHost, zParams, nullptr);
    }
}

std::string RPCClient::sendPOST(const std::string& zHost, const std::string& zParams, const std::string& zType) {
    std::vector<std::string> headers;
    if (!zType.empty()) {
        headers.push_back("Content-Type: " + zType);
    }

    auto result = performRequest(zHost, "POST", headers, &zParams, true /*follow redirects*/, 10000);
    const long code = result.second;
    if (code == 200) {
        return result.first;
    }

    std::ostringstream oss;
    oss << "POST request not HTTP_OK resp:" << code << " @ " << zHost << " params " << zParams;
    throw std::runtime_error(oss.str());
}

std::string RPCClient::sendPOSTHTTPS(const std::string& zHost, const std::string& zParams, const std::string& zType) {
    std::vector<std::string> headers;
    if (!zType.empty()) {
        headers.push_back("Content-Type: " + zType);
    }

    auto result = performRequest(zHost, "POST", headers, &zParams, true /*follow redirects*/, 10000);
    const long code = result.second;
    if (code == 200) {
        return result.first;
    }

    std::ostringstream oss;
    oss << "POST request not HTTP_OK resp:" << code << " @ " << zHost << " params " << zParams;
    throw std::runtime_error(oss.str());
}

std::string RPCClient::sendGETAuth(const std::string& zHost, const std::string& zAuthToken) {
    std::vector<std::string> headers;
    headers.push_back("Authorization: Basic " + zAuthToken);

    auto result = performRequest(zHost, "GET", headers, nullptr, true /*follow redirects*/, 10000);
    const long code = result.second;
    if (code == 200) {
        return result.first;
    } else {
        print_http_not_ok("GET", code, zHost);
        return "";
    }
}

} // namespace utils
} // namespace minima
} // namespace org