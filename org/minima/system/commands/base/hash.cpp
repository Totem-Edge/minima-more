#include "org/minima/system/commands/base/hash.hpp"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/crypto.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
// Assuming JSONArray header exists at this path based on project structure
#include "org/minima/utils/json/j_s_o_n_array.hpp"

namespace fs = std::filesystem;

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniString;
using org::minima::system::commands::CommandException;
using org::minima::utils::Crypto;
using org::minima::utils::MiniFile;
using org::minima::utils::json::JSONObject;
using org::minima::utils::json::JSONArray;

hash::hash()
    : org::minima::system::commands::Command(
          "hash",
          "(data:) (file:) (type:sha2|sha3)- Hash the data - default SHA3") {}

std::string hash::getFullHelp() const {
    return std::string("\nhash\n")
        + "\n"
        + "Hash the data or file - default SHA3.\n"
        + "\n"
        + "Returns the hash of the data provided using the algorithm specified.\n"
        + "\n"
        + "data:\n"
        + "    The data to hash. Can be HEX (0x) or a string in quotes.\n"
        + "    String data will return the the byte representation of the string.\n"
        + "\n"
        + "file:\n"
        + "    The file path - can be the full path or relative to your base folder\n"
        + "\n"
        + "type: (optional)\n"
        + "    sha2 or sha3. The hashing algorithm to use, default is SHA3.\n"
        + "    BTC and ETH support sha2 or sha3.\n"
        + "\n"
        + "Examples:\n"
        + "\n"
        + "hash data:0x1C8AFF950685C2ED4BC3174F3472287B56D9517B9C948127319A09A7A36DEAC8\n"
        + "\n"
        + "hash file:myfile.txt\n"
        + "\n"
        + "hash data:\"this is my secret\" type:sha2\n";
}

std::vector<std::string> hash::getValidParams() const {
    return std::vector<std::string>{ "data", "type", "file" };
}

std::unique_ptr<JSONObject> hash::runCommand() {
    auto ret = getJSONReply();

    std::string datastr;

    // Data to hash
    MiniData data;

    bool isfile = false;
    fs::path datafile;

    if (existsParam("file")) {
        isfile = true;
        datafile = org::minima::utils::MiniFile::createBaseFile(getParam("file"));
        if (!fs::exists(datafile)) {
            throw CommandException(
                std::string("File doesn't exist : ") + fs::absolute(datafile).string());
        }

        // Load the file bytes
        std::vector<std::uint8_t> filebytes = MiniFile::readCompleteFile(datafile);
        data = MiniData(filebytes);
    } else {
        if (isParamJSONObject("data")) {
            auto jobj = getJSONObjectParam("data");
            datastr = jobj ? jobj->toString() : std::string();
        } else if (isParamJSONArray("data")) {
            auto jarr = getJSONArrayParam("data");
            // JSONArray should support toString similar to JSONObject
            datastr = jarr ? jarr->toString() : std::string();
        } else {
            datastr = getParam("data"); // will throw if missing or blank per Command logic
        }

        // Hex or string
        if (datastr.size() >= 2 && datastr.rfind("0x", 0) == 0) {
            data = MiniData(datastr);
        } else {
            MiniString ms(datastr);
            data = MiniData(ms.getData());
        }
    }

    std::string hashtype = getParam("type", "sha3");

    std::vector<std::uint8_t> hashbytes;
    if (hashtype == "sha2") {
        hashbytes = Crypto::getInstance().hashSHA2(data.getBytes());
    } else if (hashtype == "sha3") {
        hashbytes = Crypto::getInstance().hashData(data.getBytes());
    } else {
        throw CommandException(std::string("Invalid hash type : ") + hashtype);
    }

    JSONObject resp;
    if (isfile) {
        // File info
        std::string fname   = datafile.filename().string();
        std::string abspath = fs::absolute(datafile).string();

        std::uintmax_t fsize = 0;
        try {
            fsize = fs::file_size(datafile);
        } catch (...) {
            // Mimic Java's File.length() which returns 0 if not a file or error
            fsize = 0;
        }

        resp.put("file", fname);
        resp.put("path", abspath);
        resp.put("size", static_cast<unsigned long long>(fsize));
    } else {
        resp.put("input", datastr);
        resp.put("data", data.to0xString());
    }

    resp.put("type", hashtype);
    resp.put("hash", MiniData(hashbytes).to0xString());

    ret->put("response", resp);

    return ret;
}

org::minima::system::commands::Command* hash::getFunction() {
    return new hash();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org