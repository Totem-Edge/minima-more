#include "org/minima/system/commands/base/seedrandom.hpp"

#include <utility>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/wallet/wallet.hpp"
// Note: Do NOT include seed_row.hpp to avoid redefinition conflict.
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/crypto.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::database::MinimaDB;
using org::minima::database::wallet::SeedRow;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniString;
using org::minima::system::commands::CommandException;
using org::minima::utils::Crypto;
using org::minima::utils::json::JSONObject;

seedrandom::seedrandom()
    : org::minima::system::commands::Command(
          "seedrandom",
          "[modifier:] - Generate a random value, based on your SEED and a modifier") {}

std::string seedrandom::getFullHelp() const {
    return std::string("\nseedrandom\n"
                       "\n"
                       "Generate a random value, based on your SEED and a modifier.\n"
                       "\n"
                       "modifier: \n"
                       "    The modifier - added to seed before hash.\n"
                       "\n"
                       "Examples:\n"
                       "\n"
                       "seedrandom modifier:\"Hello you\"\n"
                       "\n");
}

std::vector<std::string> seedrandom::getValidParams() const {
    return std::vector<std::string>{ "modifier" };
}

std::unique_ptr<JSONObject> seedrandom::runCommand() {
    // Top-level reply object
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Check not locked
    if (!MinimaDB::getDB()->getWallet().isBaseSeedAvailable()) {
        throw CommandException("DB locked!");
    }

    // Get modifier
    std::string modifier = getParam("modifier");

    // Convert to MiniData via MiniString
    MiniString mstr(modifier);
    std::unique_ptr<MiniData> moddata_ptr = MiniData::getMiniDataVersion(mstr);
    // Java code assumes this succeeds; dereference directly
    MiniData& moddata = *moddata_ptr;

    // Make it different from defaults
    MiniData deadmod("0xDEADDEAD");
    MiniData hashmod = Crypto::getInstance().hashObjects(moddata, deadmod);

    // Get the base seed
    SeedRow sr = MinimaDB::getDB()->getWallet().getBaseSeed();

    // Hash them together
    MiniData seeddata(sr.getSeed());
    MiniData hash = Crypto::getInstance().hashObjects(hashmod, seeddata);

    // Build response
    JSONObject resp;
    resp.put("modifier", modifier);
    resp.put("seedrandom", hash.to0xString());

    ret->put("response", resp);
    return ret;
}

org::minima::system::commands::Command* seedrandom::getFunction() {
    return new seedrandom();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org