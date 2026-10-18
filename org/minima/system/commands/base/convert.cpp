#include "org/minima/system/commands/base/convert.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/objects/address.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

namespace {

// Lowercase utility for ASCII
static std::string toLowerAscii(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// Base64 encoding/decoding utilities (strict, RFC 4648 alphabet)
static const char B64_ALPHABET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64Encode(const std::vector<std::uint8_t>& data) {
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);

    std::size_t i = 0;
    while (i + 3 <= data.size()) {
        std::uint32_t block = (static_cast<std::uint32_t>(data[i]) << 16) |
                              (static_cast<std::uint32_t>(data[i + 1]) << 8) |
                              (static_cast<std::uint32_t>(data[i + 2]));
        i += 3;

        out.push_back(B64_ALPHABET[(block >> 18) & 0x3F]);
        out.push_back(B64_ALPHABET[(block >> 12) & 0x3F]);
        out.push_back(B64_ALPHABET[(block >> 6) & 0x3F]);
        out.push_back(B64_ALPHABET[block & 0x3F]);
    }

    std::size_t rem = data.size() - i;
    if (rem == 1) {
        std::uint32_t block = static_cast<std::uint32_t>(data[i]) << 16;
        out.push_back(B64_ALPHABET[(block >> 18) & 0x3F]);
        out.push_back(B64_ALPHABET[(block >> 12) & 0x3F]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2) {
        std::uint32_t block = (static_cast<std::uint32_t>(data[i]) << 16) |
                              (static_cast<std::uint32_t>(data[i + 1]) << 8);
        out.push_back(B64_ALPHABET[(block >> 18) & 0x3F]);
        out.push_back(B64_ALPHABET[(block >> 12) & 0x3F]);
        out.push_back(B64_ALPHABET[(block >> 6) & 0x3F]);
        out.push_back('=');
    }

    return out;
}

static inline int b64Index(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static std::vector<std::uint8_t> base64Decode(const std::string& input) {
    if (input.empty()) return {};

    if (input.size() % 4 != 0) {
        throw std::invalid_argument("Invalid Base64 input length");
    }

    std::size_t padding = 0;
    if (input.size() >= 2) {
        if (input[input.size() - 1] == '=') padding++;
        if (input[input.size() - 2] == '=') padding++;
    }

    std::size_t out_len = (input.size() / 4) * 3 - padding;
    std::vector<std::uint8_t> out;
    out.reserve(out_len);

    for (std::size_t i = 0; i < input.size(); i += 4) {
        int a = (input[i] == '=') ? -1 : b64Index(input[i]);
        int b = (input[i + 1] == '=') ? -1 : b64Index(input[i + 1]);
        int c = (input[i + 2] == '=') ? -1 : b64Index(input[i + 2]);
        int d = (input[i + 3] == '=') ? -1 : b64Index(input[i + 3]);

        if (a < 0 || b < 0 || (c < 0 && input[i + 2] != '=') || (d < 0 && input[i + 3] != '=')) {
            throw std::invalid_argument("Invalid Base64 character");
        }

        std::uint32_t block = (static_cast<std::uint32_t>(a) << 18) |
                              (static_cast<std::uint32_t>(b) << 12) |
                              ((c >= 0 ? static_cast<std::uint32_t>(c) : 0) << 6) |
                              ((d >= 0 ? static_cast<std::uint32_t>(d) : 0));

        out.push_back(static_cast<std::uint8_t>((block >> 16) & 0xFF));
        if (input[i + 2] != '=') {
            out.push_back(static_cast<std::uint8_t>((block >> 8) & 0xFF));
        }
        if (input[i + 3] != '=') {
            out.push_back(static_cast<std::uint8_t>(block & 0xFF));
        }
    }

    return out;
}

} // anonymous namespace

convert::convert()
    : org::minima::system::commands::Command(
          "convert",
          "[from:] [to:] [data:] - Convert between different data types (String, HEX, Mx, Base64)") {}

std::string convert::getFullHelp() const {
    return std::string("\nconvert\n"
                       "\n"
                       "Convert between different data types\n"
                       "\n"
                       "Returns converted data.\n"
                       "\n"
                       "from:\n"
                       "    The type of the data param.\n"
                       "\n"
                       "to:\n"
                       "    The type you want to convert to.\n"
                       "\n"
                       "data:\n"
                       "    The the data to convert.\n"
                       "\n"
                       "Examples:\n"
                       "\n"
                       "convert from:String to:HEX data:hello\n"
                       "\n"
                       "convert from:HEX to:Mx data:0XFFFF\n"
                       "\n"
                       "convert from:String to:Base64 data:hello\n");
}

std::vector<std::string> convert::getValidParams() const {
    return std::vector<std::string>{ "from", "to", "data" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> convert::runCommand() {
    using org::minima::objects::Address;
    using org::minima::objects::base::MiniData;
    using org::minima::objects::base::MiniString;
    using org::minima::utils::json::JSONObject;
    using org::minima::utils::json::JSONArray;
    using org::minima::system::commands::CommandException;

    std::unique_ptr<JSONObject> ret = getJSONReply();

    std::string from = toLowerAscii(getParam("from"));
    std::string to   = toLowerAscii(getParam("to"));

    std::string data;
    if (isParamJSONObject("data")) {
        auto jptr = getJSONObjectParam("data");
        data = jptr ? jptr->toString() : std::string{};
    } else if (isParamJSONArray("data")) {
        auto jarr = getJSONArrayParam("data");
        data = jarr ? jarr->toString() : std::string{};
    } else {
        data = getParam("data");
    }

    JSONObject resp;
    MiniData fromdata;
    std::string todata;

    try {
        // Determine initial MiniData from "from" type
        if (from == "hex") {
            fromdata = MiniData(data);
        } else if (from == "mx") {
            fromdata = Address::convertMinimaAddress(data);
        } else if (from == "string") {
            MiniString ms(data);
            fromdata = MiniData(ms.getData());
        } else if (from == "base64") {
            auto bytes = base64Decode(data);
            fromdata = MiniData(bytes);
        } else {
            throw CommandException(std::string("Invalid FROM type : ") + from);
        }

        // Convert to requested "to" type
        if (to == "hex") {
            todata = fromdata.to0xString();
        } else if (to == "mx") {
            todata = Address::makeMinimaAddress(fromdata);
        } else if (to == "string") {
            MiniString ms(fromdata.getBytes());
            todata = ms.toString();
        } else if (to == "base64") {
            todata = base64Encode(fromdata.getBytes());
        } else {
            throw CommandException(std::string("Invalid TO type : ") + to);
        }

    } catch (const CommandException&) {
        throw; // rethrow as-is
    } catch (const std::exception& exc) {
        throw CommandException(exc.what());
    } catch (...) {
        throw CommandException("Unknown error during conversion");
    }

    // Add to response
    resp.put("conversion", todata);
    ret->put("response", resp);

    return ret;
}

org::minima::system::commands::Command* convert::getFunction() {
    return new convert();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org