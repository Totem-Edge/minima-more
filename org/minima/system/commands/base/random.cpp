#include "org/minima/system/commands/base/random.hpp"

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/utils/base_converter.hpp"
#include "org/minima/utils/crypto.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#include <memory>
#include <string>
#include <vector>

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::utils::Crypto;
using org::minima::utils::BaseConverter;
using org::minima::utils::json::JSONObject;

random::random()
    : Command("random", "(size:) (type:) - Generate a random hash value, defaults to 32 bytes") {
}

std::string random::getFullHelp() const {
    return std::string("\nrandom\n")
         + "\n"
         + "Generate a random hash value, defaults to 32 bytes.\n"
         + "\n"
         + "size: (optional)\n"
         + "    Integer number of bytes for the hash value.\n"
         + "\n"
         + "type: (optional)\n"
         + "    sha3 (default) or sha2.\n"
         + "\n"
         + "Examples:\n"
         + "\n"
         + "random\n"
         + "\n"
         + "random size:64\n";
}

std::vector<std::string> random::getValidParams() const {
    return { "size", "type" };
}

std::unique_ptr<JSONObject> random::runCommand() {
    // Prepare base reply
    auto ret = getJSONReply();

    // How big a random number (default 32)
    MiniNumber defsize(32);
    std::unique_ptr<MiniNumber> size = getNumberParam("size", defsize);

    // Generate random data
    MiniData rand = MiniData::getRandomData(size->getAsInt());

    // Hash type (default sha3)
    std::string hashtype = getParam("type", "sha3");

    // Compute hash
    std::vector<std::uint8_t> hash;
    if (hashtype == "sha2") {
        hash = Crypto::getInstance().hashSHA2(rand.getBytes());
    } else if (hashtype == "sha3") {
        hash = Crypto::getInstance().hashData(rand.getBytes());
    } else {
        throw org::minima::system::commands::CommandException("Invalid hash type : " + hashtype);
    }

    // Wrap hash in MiniData
    MiniData randhash(hash);

    // Build response JSON
    JSONObject resp;
    resp.put("size", size->toString());
    resp.put("random", rand.to0xString());
    resp.put("hashed", randhash.to0xString());
    resp.put("type", hashtype);

    // Optionally add keycode if size >= 16
    if (size->getAsInt() >= 16) {
        std::string b32 = BaseConverter::encode32(rand.getBytes());

        // Replicate Java substring indices exactly
        // b32.substring(2,6) etc. -> std::string::substr(pos, len)
        std::string mm = b32.substr(2, 4)  + "-"
                       + b32.substr(7, 4)  + "-"
                       + b32.substr(12, 4) + "-"
                       + b32.substr(17, 4) + "-"
                       + b32.substr(22, 4);

        resp.put("keycode", mm);
    }

    // Attach response
    ret->put("response", resp);

    return ret;
}

org::minima::system::commands::Command* random::getFunction() {
    return new random();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org