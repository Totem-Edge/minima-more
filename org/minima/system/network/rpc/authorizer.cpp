#include "org/minima/system/network/rpc/authorizer.hpp"

// Project includes (full definitions required in .cpp)
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/user_d_b.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#include <cctype>
#include <stdexcept>
#include <vector>
#include <any>
#include <algorithm>

namespace {

// Trim ASCII whitespace from both ends
inline std::string trim_ascii(const std::string& s) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    std::string::size_type start = 0;
    while (start < s.size() && is_space(static_cast<unsigned char>(s[start]))) ++start;
    std::string::size_type end = s.size();
    while (end > start && is_space(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

// Base64 decoding table and function (strict RFC 4648 without whitespace)
inline int b64_index(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    if (c == '=') return -2; // padding
    return -1; // invalid
}

std::vector<uint8_t> base64_decode_strict(const std::string& input) {
    // Remove any whitespace (Java's Base64.getDecoder() rejects line separators; 
    // here we forbid whitespace to emulate strict behavior; if present, throw).
    for (char c : input) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            throw std::runtime_error("Invalid Base64: contains whitespace");
        }
    }

    if (input.empty()) return {};

    if (input.size() % 4 != 0) {
        throw std::runtime_error("Invalid Base64 length");
    }

    std::vector<uint8_t> out;
    out.reserve((input.size() / 4) * 3);

    for (std::size_t i = 0; i < input.size(); i += 4) {
        int vals[4];
        for (int j = 0; j < 4; ++j) {
            int v = b64_index(input[i + j]);
            if (v == -1) {
                throw std::runtime_error("Invalid Base64 character");
            }
            vals[j] = v;
        }

        // Decode block
        int v0 = vals[0];
        int v1 = vals[1];
        int v2 = vals[2];
        int v3 = vals[3];

        if (v0 < 0 || v1 < 0) {
            throw std::runtime_error("Invalid Base64 padding in first two chars");
        }

        uint32_t triple = (static_cast<uint32_t>(v0) << 18) |
                          (static_cast<uint32_t>(v1) << 12) |
                          ((v2 >= 0 ? static_cast<uint32_t>(v2) : 0) << 6) |
                          ((v3 >= 0 ? static_cast<uint32_t>(v3) : 0));

        out.push_back(static_cast<uint8_t>((triple >> 16) & 0xFF));

        if (v2 >= 0) {
            out.push_back(static_cast<uint8_t>((triple >> 8) & 0xFF));
        } else {
            // Must be padding '=='
            if (!(v2 == -2 && v3 == -2)) {
                throw std::runtime_error("Invalid Base64 padding");
            }
            continue;
        }

        if (v3 >= 0) {
            out.push_back(static_cast<uint8_t>(triple & 0xFF));
        } else {
            // Must be single padding '='
            if (v3 != -2) {
                throw std::runtime_error("Invalid Base64 padding");
            }
        }
    }

    return out;
}

} // anonymous namespace

namespace org {
namespace minima {
namespace system {
namespace network {
namespace rpc {

using org::minima::database::MinimaDB;
using org::minima::database::userprefs::UserDB;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;
using org::minima::system::params::GeneralParams;
using org::minima::utils::MinimaLogger;

JSONObject Authorizer::checkAuchCredentials(const std::string& zAuthHeader) {
    JSONObject falseret;
    falseret.put("valid", false);

    JSONObject ret;
    ret.put("valid", false);

    try {
        // Access the UserDB and mimic the side-effect of size() call in Java
        UserDB& userdb = MinimaDB::getDB()->getUserDB();
        int rpcusers = static_cast<int>(userdb.getRPCUsers().size());
        (void)rpcusers; // Silence unused variable warning while preserving behavior

        // Are we BASIC checking
        if (GeneralParams::RPC_AUTHSTYLE == "basic") {
            // Is it basic Auth
            std::size_t pos = zAuthHeader.find("Basic ");
            if (pos != std::string::npos) {
                std::string userpass = zAuthHeader.substr(pos + 6);

                // Base64 decode
                std::vector<uint8_t> dec = base64_decode_strict(userpass);

                // Construct string from bytes and trim
                std::string decstr(dec.begin(), dec.end());
                decstr = trim_ascii(decstr);

                // Get the 2 bits..
                std::size_t col = decstr.find(':');
                if (col == std::string::npos) {
                    // Mirror Java behavior where substring with invalid index throws
                    throw std::runtime_error("Invalid Basic auth format, missing ':'");
                }
                std::string user = decstr.substr(0, col);
                std::string password = decstr.substr(col + 1);

                ret.put("username", user);

                // Now check
                if (user == "minima") {
                    if (!GeneralParams::RPC_AUTHENTICATE || password == GeneralParams::RPC_PASSWORD) {
                        ret.put("valid", true);
                        ret.put("mode", std::string("write"));
                        return ret;
                    }
                } else {
                    if (GeneralParams::RPC_AUTHENTICATE) {
                        JSONArray users = userdb.getRPCUsers();
                        for (std::size_t i = 0; i < users.size(); ++i) {
                            const std::any& userobj = users.at(i);
                            // Attempt to treat as JSONObject
                            if (userobj.type() == typeid(JSONObject)) {
                                const JSONObject& rpcuser = std::any_cast<const JSONObject&>(userobj);

                                // Is it the one to be removed..
                                if (rpcuser.getString("username") == user &&
                                    rpcuser.getString("password") == password) {
                                    ret.put("valid", true);
                                    ret.put("mode", rpcuser.getString("mode"));
                                }
                            }
                        }
                        return ret;
                    } else {
                        // Cannot access extra users if no Auth for main minima user
                        MinimaLogger::log("[!] Cannot access rpc user (" + user + ") as no default password (for user minima) set via -rpcpassword..");
                    }
                }
            }
        }
    } catch (const std::exception& exc) {
        MinimaLogger::log(exc);
        return falseret;
    }

    return falseret;
}

} // namespace rpc
} // namespace network
} // namespace system
} // namespace minima
} // namespace org