#include "org/minima/utils/minima_r_p_c_client.hpp"

#include <algorithm>
#include <any>
#include <cctype>
#include <chrono>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
//   #define NOMINMAX
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
#else
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <sys/socket.h>
  #include <sys/types.h>
  #include <unistd.h>
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/evp.h>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/parser/j_s_o_n_parser.hpp"
#include "org/minima/utils/json/parser/parse_exception.hpp"

// Local helpers are kept in an anonymous namespace
namespace {

void MinimaLogger_log(const std::string& msg) {
    std::cerr << msg << std::endl;
}
void MinimaLogger_log(const std::exception& ex) {
    std::cerr << "Exception: " << ex.what() << std::endl;
}

#ifdef _WIN32
std::once_flag g_wsa_once;
void ensure_winsock() {
    std::call_once(g_wsa_once, []() {
        WSADATA wsaData;
        int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (res != 0) {
            throw std::runtime_error("WSAStartup failed: " + std::to_string(res));
        }
    });
}
#else
void ensure_winsock() {}
#endif

std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

// Java URLEncoder.encode(s, "UTF-8") behavior approximation:
// - Space ' ' -> '+'
// - Unreserved chars [A-Za-z0-9-_.*] unchanged
// - All others percent-encoded with uppercase hex
std::string url_encode_form(const std::string& input_utf8) {
    std::ostringstream oss;
    for (unsigned char c : input_utf8) {
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '*') {
            oss << static_cast<char>(c);
        } else if (c == ' ') {
            oss << '+';
        } else {
            oss << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)c;
            oss << std::nouppercase << std::dec;
        }
    }
    return oss.str();
}

std::string base64_encode(const std::string& in) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i + 3 <= in.size()) {
        unsigned char a = in[i++];
        unsigned char b = in[i++];
        unsigned char c = in[i++];
        out.push_back(tbl[(a >> 2) & 0x3F]);
        out.push_back(tbl[((a & 0x3) << 4) | ((b >> 4) & 0xF)]);
        out.push_back(tbl[((b & 0xF) << 2) | ((c >> 6) & 0x3)]);
        out.push_back(tbl[c & 0x3F]);
    }

    if (i + 1 == in.size()) {
        unsigned char a = in[i++];
        out.push_back(tbl[(a >> 2) & 0x3F]);
        out.push_back(tbl[((a & 0x3) << 4)]);
        out.push_back('=');
        out.push_back('=');
    } else if (i + 2 == in.size()) {
        unsigned char a = in[i++];
        unsigned char b = in[i++];
        out.push_back(tbl[(a >> 2) & 0x3F]);
        out.push_back(tbl[((a & 0x3) << 4) | (b >> 4)]);
        out.push_back(tbl[((b & 0xF) << 2)]);
        out.push_back('=');
    }
    return out;
}

struct ParsedURL {
    std::string scheme; // "http" or "https"
    std::string host;
    int port;
    std::string path; // includes leading '/', at least "/"
};

ParsedURL parse_url(const std::string& url) {
    ParsedURL p;
    size_t pos = url.find("://");
    if (pos == std::string::npos) {
        throw std::invalid_argument("Invalid URL (no scheme): " + url);
    }
    p.scheme = url.substr(0, pos);
    size_t host_start = pos + 3;

    size_t path_start = url.find('/', host_start);
    std::string hostport;
    if (path_start == std::string::npos) {
        hostport = url.substr(host_start);
        p.path = "/";
    } else {
        hostport = url.substr(host_start, path_start - host_start);
        p.path = url.substr(path_start);
    }
    if (p.path.empty()) p.path = "/";

    size_t colon = hostport.find(':');
    if (colon == std::string::npos) {
        p.host = hostport;
        p.port = (p.scheme == "https") ? 443 : 80;
    } else {
        p.host = hostport.substr(0, colon);
        std::string portstr = hostport.substr(colon + 1);
        if (portstr.empty()) {
            p.port = (p.scheme == "https") ? 443 : 80;
        } else {
            p.port = std::stoi(portstr);
        }
    }
    return p;
}

int connect_tcp(const std::string& host, int port) {
    ensure_winsock();

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;

    std::string portstr = std::to_string(port);
    struct addrinfo* res = nullptr;
    int ret = getaddrinfo(host.c_str(), portstr.c_str(), &hints, &res);
    if (ret != 0 || !res) {
#ifdef _WIN32
        throw std::runtime_error("getaddrinfo failed: " + std::to_string(ret));
#else
        throw std::runtime_error(std::string("getaddrinfo failed: ") + gai_strerror(ret));
#endif
    }

    int sockfd = -1;
    for (struct addrinfo* rp = res; rp != nullptr; rp = rp->ai_next) {
        int s = static_cast<int>(socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol));
        if (s < 0) continue;

        if (connect(s, rp->ai_addr, static_cast<int>(rp->ai_addrlen)) == 0) {
            sockfd = s;
            break;
        }
#ifdef _WIN32
        closesocket(s);
#else
        close(s);
#endif
    }

    freeaddrinfo(res);

    if (sockfd < 0) {
        throw std::runtime_error("Unable to connect to " + host + ":" + std::to_string(port));
    }
    return sockfd;
}

std::string read_all_from_socket(int sockfd) {
    std::string data;
    char buf[4096];
    for (;;) {
#ifdef _WIN32
        int n = recv(sockfd, buf, sizeof(buf), 0);
#else
        ssize_t n = recv(sockfd, buf, sizeof(buf), 0);
#endif
        if (n <= 0) break;
        data.append(buf, buf + n);
    }
    return data;
}

std::string read_all_from_ssl(SSL* ssl) {
    std::string data;
    char buf[4096];
    for (;;) {
        int n = SSL_read(ssl, buf, static_cast<int>(sizeof(buf)));
        if (n <= 0) {
            int err = SSL_get_error(ssl, n);
            if (err == SSL_ERROR_ZERO_RETURN) break; // clean shutdown
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) continue;
            break;
        }
        data.append(buf, buf + n);
    }
    return data;
}

std::string to_lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c){ return std::tolower(c); });
    return out;
}

std::string parse_http_body(const std::string& raw) {
    // Split headers and body
    size_t hdrend = raw.find("\r\n\r\n");
    size_t alt = raw.find("\n\n");
    size_t header_end = std::string::npos;
    if (hdrend != std::string::npos) header_end = hdrend + 4;
    else if (alt != std::string::npos) header_end = alt + 2;
    else return raw; // no headers? return as is

    std::string header = raw.substr(0, header_end);
    std::string body = raw.substr(header_end);

    std::string header_lower = to_lower(header);
    if (header_lower.find("transfer-encoding: chunked") != std::string::npos) {
        // Dechunk
        std::string out;
        size_t pos = 0;
        while (true) {
            // find line end
            size_t line_end = body.find("\r\n", pos);
            if (line_end == std::string::npos) break;
            std::string size_str = body.substr(pos, line_end - pos);
            // allow optional extensions after ';'
            size_t sc = size_str.find(';');
            if (sc != std::string::npos) size_str = size_str.substr(0, sc);
            // parse hex size
            size_t chunk_size = 0;
            std::istringstream iss(size_str);
            iss >> std::hex >> chunk_size;
            pos = line_end + 2;
            if (chunk_size == 0) {
                // final chunk, optional trailer till CRLF CRLF
                // skip trailing CRLF
                // find final \r\n
                // We're done
                break;
            }
            if (pos + chunk_size > body.size()) break;
            out.append(body.substr(pos, chunk_size));
            pos += chunk_size;
            // skip CRLF after chunk
            if (pos + 2 <= body.size() && body.substr(pos, 2) == "\r\n") pos += 2;
            else break;
        }
        return out;
    }

    return body;
}

std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::string s = hex;
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s = s.substr(2);
    }
    if (s.size() % 2 != 0) throw std::invalid_argument("Invalid hex length");
    std::vector<uint8_t> out;
    out.reserve(s.size() / 2);
    auto hexval = [](char c)->int{
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };
    for (size_t i = 0; i < s.size(); i += 2) {
        int hi = hexval(s[i]);
        int lo = hexval(s[i+1]);
        if (hi < 0 || lo < 0) throw std::invalid_argument("Invalid hex character");
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return out;
}

std::vector<uint8_t> get_peer_pubkey_der(X509* cert) {
    std::vector<uint8_t> out;
    if (!cert) return out;
    EVP_PKEY* pkey = X509_get_pubkey(cert);
    if (!pkey) return out;
    int len = i2d_PUBKEY(pkey, nullptr);
    if (len <= 0) {
        EVP_PKEY_free(pkey);
        return out;
    }
    out.resize(len);
    unsigned char* p = out.data();
    int len2 = i2d_PUBKEY(pkey, &p);
    EVP_PKEY_free(pkey);
    if (len2 != len) {
        out.clear();
    }
    return out;
}

std::string http_request_basic_auth(const std::string& url, const std::string& username, const std::string& password) {
    ParsedURL p = parse_url(url);

    std::ostringstream req;
    req << "GET " << p.path << " HTTP/1.1\r\n";
    req << "Host: " << p.host << ":" << p.port << "\r\n";
    req << "Connection: close\r\n";
    std::string auth = base64_encode(username + ":" + password);
    req << "Authorization: Basic " << auth << "\r\n";
    req << "\r\n";
    std::string request = req.str();

    int sockfd = connect_tcp(p.host, p.port);
    std::string raw;

    try {
        // Send
#ifdef _WIN32
        int sent = send(sockfd, request.c_str(), static_cast<int>(request.size()), 0);
        if (sent == SOCKET_ERROR) {
            throw std::runtime_error("send failed");
        }
#else
        ssize_t sent = send(sockfd, request.c_str(), request.size(), 0);
        if (sent < 0) {
            throw std::runtime_error("send failed");
        }
#endif
        // Read all
        raw = read_all_from_socket(sockfd);
    } catch (...) {
#ifdef _WIN32
        closesocket(sockfd);
#else
        close(sockfd);
#endif
        throw;
    }

#ifdef _WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif

    return parse_http_body(raw);
}

std::string https_request_basic_auth_with_pin(const std::string& url, const std::string& username, const std::string& password, const std::string& sslpubkey_hex) {
    ParsedURL p = parse_url(url);

    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        throw std::runtime_error("SSL_CTX_new failed");
    }

    // If no pin, disable peer verification (matches typical custom trust manager behavior).
    bool have_pin = !sslpubkey_hex.empty();
    if (!have_pin) {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    } else {
        // We'll verify after handshake by comparing the public key DER bytes.
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    }

    int sockfd = -1;
    SSL* ssl = nullptr;
    std::string raw;

    try {
        sockfd = connect_tcp(p.host, p.port);

        ssl = SSL_new(ctx);
        if (!ssl) throw std::runtime_error("SSL_new failed");

        if (SSL_set_tlsext_host_name(ssl, p.host.c_str()) != 1) {
            throw std::runtime_error("SSL_set_tlsext_host_name failed");
        }

        SSL_set_fd(ssl, sockfd);

        if (SSL_connect(ssl) != 1) {
            throw std::runtime_error("SSL_connect failed");
        }

        if (have_pin) {
            std::vector<uint8_t> want = hex_to_bytes(sslpubkey_hex);
            X509* cert = SSL_get_peer_certificate(ssl);
            if (!cert) {
                throw std::runtime_error("No server certificate presented");
            }
            std::vector<uint8_t> got = get_peer_pubkey_der(cert);
            X509_free(cert);

            if (got.empty() || got.size() != want.size() || !std::equal(got.begin(), got.end(), want.begin())) {
                throw std::runtime_error("Server public key does not match pinned key");
            }
        }

        // Build request
        std::ostringstream req;
        req << "GET " << p.path << " HTTP/1.1\r\n";
        req << "Host: " << p.host << ":" << p.port << "\r\n";
        req << "Connection: close\r\n";
        std::string auth = base64_encode(username + ":" + password);
        req << "Authorization: Basic " << auth << "\r\n";
        req << "\r\n";
        std::string request = req.str();

        int written = SSL_write(ssl, request.c_str(), static_cast<int>(request.size()));
        if (written <= 0) {
            throw std::runtime_error("SSL_write failed");
        }

        raw = read_all_from_ssl(ssl);

        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = nullptr;

#ifdef _WIN32
        closesocket(sockfd);
#else
        close(sockfd);
#endif
        sockfd = -1;

        SSL_CTX_free(ctx);

        return parse_http_body(raw);
    } catch (...) {
        if (ssl) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
            ssl = nullptr;
        }
        if (sockfd >= 0) {
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
            sockfd = -1;
        }
        SSL_CTX_free(ctx);
        throw;
    }
}

// Simple JSON pretty-printer from a minified JSON string.
// This does not validate; it assumes 'json' is valid JSON.
std::string json_pretty_print(const std::string& json) {
    std::ostringstream out;
    int indent = 0;
    bool in_string = false;
    bool escape = false;

    auto newline_indent = [&](int level) {
        out << '\n';
        for (int i = 0; i < level; ++i) out << "  ";
    };

    for (size_t i = 0; i < json.size(); ++i) {
        char c = json[i];

        if (in_string) {
            out << c;
            if (escape) {
                escape = false;
            } else {
                if (c == '\\') escape = true;
                else if (c == '"') in_string = false;
            }
            continue;
        }

        switch (c) {
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                // skip whitespace outside strings
                break;

            case '"':
                in_string = true;
                out << c;
                break;

            case '{':
            case '[':
                out << c;
                indent++;
                newline_indent(indent);
                break;

            case '}':
            case ']':
                indent--;
                newline_indent(indent);
                out << c;
                break;

            case ',':
                out << c;
                newline_indent(indent);
                break;

            case ':':
                out << ": ";
                break;

            default:
                out << c;
                break;
        }
    }
    out << '\n';
    return out.str();
}

} // anonymous namespace

namespace org {
namespace minima {
namespace utils {

using org::minima::objects::base::MiniString;

static void print_banner(const std::string& host, bool ssl, bool bpass, const std::string& sslpubkey) {
    MinimaLogger_log("**********************************************");
    MinimaLogger_log("*  __  __  ____  _  _  ____  __  __    __    *");
    MinimaLogger_log("* (  \\/  )(_  _)( \\( )(_  _)(  \\/  )  /__\\   *");
    MinimaLogger_log("*  )    (  _)(_  )  (  _)(_  )    (  /(__)\\  *");
    MinimaLogger_log("* (_/\\/\\_)(____)(_)\\_)(____)(_/\\/\\_)(__)(__) *");
    MinimaLogger_log("*                                            *");
    MinimaLogger_log("**********************************************");
    MinimaLogger_log("Welcome to the Minima RPCClient - for assistance type help. Then press enter.");
    MinimaLogger_log("host        :" + host);
    MinimaLogger_log(std::string("ssl         :") + (ssl ? "true" : "false"));
    MinimaLogger_log(std::string("usepassword :") + (bpass ? "true" : "false"));
    MinimaLogger_log("sslpubkey   :" + sslpubkey);
    MinimaLogger_log("To exit this app use 'exit'. 'quit' will shutdown Minima");
}

int MinimaRPCClient::run(int argc, char* argv[]) {
    std::string host = "http://127.0.0.1:9005";
    bool bpass = false;
    std::string username = "minima";
    std::string password = "password";
    std::string sslpubkey;

    std::string command;

    // Parse args
    {
        int counter = 1;
        while (counter < argc) {
            std::string arg = argv[counter++];
            if (arg == "-host" && counter < argc) {
                host = argv[counter++];
            } else if (arg == "-username" && counter < argc) {
                username = argv[counter++];
            } else if (arg == "-password" && counter < argc) {
                password = argv[counter++];
                bpass = true;
            } else if (arg == "-sslpubkey" && counter < argc) {
                sslpubkey = argv[counter++];
            } else if (arg == "-command" && counter < argc) {
                command = argv[counter++];
            } else if (arg == "-help") {
                std::cout << "MinimaRPCClient Help\n";
                std::cout << " -host       : Specify the host IP:PORT\n";
                std::cout << " -password   : Specify the RPC Basic AUTH password (use with SSL)\n";
                std::cout << " -username   : Specify the RPC Basic AUTH Username (defaults to minima)\n";
                std::cout << " -command    : Specify a single command to run\n";
                std::cout << " -sslpubkey  : The SSL public key from Minima rpc command ( if using SSL )\n";
                std::cout << " -help       : Print this help\n";
                return 1;
            } else {
                std::cout << "Unknown parameter : " << arg << std::endl;
                return 1;
            }
        }
    }

    bool ssl = false;
    if (host.rfind("https://", 0) == 0) {
        ssl = true;
        // In the Java version a javax.net.ssl.SSLContext is built here with MinimaTrustManager.
        // In this C++ version we construct the TLS context inside the HTTPS request function per-call,
        // optionally enforcing server public key pinning if sslpubkey is provided.
    }

    // Make sure host ends with "/"
    if (!host.empty() && host.back() != '/') {
        host.push_back('/');
    }

    if (command.empty()) {
        print_banner(host, ssl, bpass, sslpubkey);
    }

    std::string result;

    if (!command.empty()) {
        try {
            std::string enc = url_encode_form(command); // URLEncoder.encode
            std::string full = host + enc;

            if (ssl) {
                result = https_request_basic_auth_with_pin(full, username, password, sslpubkey);
            } else {
                result = http_request_basic_auth(full, username, password);
            }

            // Parse JSON to mimic Java flow; pretty print the raw JSON.
            try {
                org::minima::utils::json::parser::JSONParser parser;
                std::any parsed = parser.parse(result);
                (void)parsed; // not used further; presence mimics Java parsing and exceptions
                std::cout << json_pretty_print(result);
            } catch (const org::minima::utils::json::parser::ParseException& e) {
                // Java: e.printStackTrace()
                MinimaLogger_log(e);
            }
        } catch (const std::exception& ex) {
            MinimaLogger_log(ex);
            return 1;
        }
    } else {
        // Interactive
        std::string input;
        for (;;) {
            try {
                if (!std::getline(std::cin, input)) {
                    break; // EOF
                }
                if (!input.empty()) {
                    input = trim(input);
                    if (input == "exit") {
                        break;
                    }

                    std::string enc = url_encode_form(input);

                    std::string full = host + enc;
                    if (ssl) {
                        result = https_request_basic_auth_with_pin(full, username, password, sslpubkey);
                    } else {
                        result = http_request_basic_auth(full, username, password);
                    }

                    try {
                        org::minima::utils::json::parser::JSONParser parser;
                        std::any parsed = parser.parse(result);
                        (void)parsed;
                        std::cout << json_pretty_print(result);
                    } catch (const std::exception& ex) {
                        MinimaLogger_log(ex);
                    }

                    if (input == "quit") {
                        break;
                    }
                }
            } catch (const std::exception& ex) {
                MinimaLogger_log(ex);
            }
        }
    }

    return 0;
}

} // namespace utils
} // namespace minima
} // namespace org

#ifdef STANDALONE_TEST
int main(int argc, char* argv[]) {
    return org::minima::utils::MinimaRPCClient::run(argc, argv);
}
#endif // STANDALONE_TEST