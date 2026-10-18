#include "org/minima/objects/greeting.hpp"

// Full headers for used types
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/parser/j_s_o_n_parser.hpp"
#include "org/minima/utils/json/parser/parse_exception.hpp"
#include "org/minima/system/params/global_params.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/brains/tx_po_w_checker.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/user_d_b.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/objects/tx_po_w.hpp"

#include <typeinfo>
#include <any>
#include <utility>

namespace org {
namespace minima {
namespace objects {

using org::minima::objects::base::MiniString;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniData;
using org::minima::utils::json::JSONObject;
using org::minima::utils::json::parser::JSONParser;
using org::minima::utils::json::parser::ParseException;
using org::minima::utils::MinimaLogger;
using org::minima::system::params::GlobalParams;
using org::minima::system::params::GeneralParams;

Greeting::Greeting()
    : mVersion(std::make_unique<MiniString>(GlobalParams::MINIMA_VERSION))
    , mExtraData(std::make_unique<JSONObject>())
    , mTopBlock(std::make_unique<MiniNumber>(MiniNumber::ZERO()))
    , mChain() {
}

Greeting::~Greeting() = default;
Greeting::Greeting(Greeting&&) noexcept = default;
Greeting& Greeting::operator=(Greeting&&) noexcept = default;

Greeting& Greeting::createGreeting() {
    // Lock the DB
    org::minima::database::MinimaDB::getDB()->readLock(true);

    try {
        //
        // FIX 1: Get userdb as a reference (auto&)
        //
        auto& userdb = org::minima::database::MinimaDB::getDB()->getUserDB();
        
        //
        // FIX 2: Remove the 'if (userdb)' check (references cannot be null)
        //
        
        //
        // FIX 3: Use the dot '.' operator on the reference
        //
        mExtraData->put("welcome", userdb.getWelcome());
        

        // What is my Host / Port
        if (GeneralParams::IS_HOST_SET) {
            mExtraData->put("host", GeneralParams::MINIMA_HOST);
        }
        mExtraData->put("port", std::to_string(GeneralParams::MINIMA_PORT));

        // Advertise the chain ID so peers on a different network are rejected
        // at the handshake instead of failing block checks later.
        mExtraData->put("chainid", org::minima::system::brains::TxPoWChecker::CURRENT_NETWORK.to0xString());

        // Get chain tip from TxPoWTree
        auto& tree = org::minima::database::MinimaDB::getDB()->getTxPoWTree();
        auto tip = tree.getTip();

        if (tip == nullptr) {
            // First time user
            setTopBlock(MiniNumber::MINUSONE());
        } else {
            setTopBlock(tip->getTxPoW().getBlockNumber());
        }

        // Walk up chain adding each TxPoWID
        while (tip != nullptr) {
            mChain.push_back(std::make_unique<MiniData>(tip->getTxPoW().getTxPoWIDData()));
            tip = tip->getParent();
        }

    } catch (const std::exception& exc) {
        MinimaLogger::log(exc);
    }

    // Unlock..
    org::minima::database::MinimaDB::getDB()->readLock(false);

    return *this;
}

JSONObject& Greeting::getExtraData() {
    return *mExtraData;
}

std::string Greeting::getExtraDataValue(const std::string& zKey) const {
    // Follow Java intent: fetch string value for key
    try {
        return mExtraData->getString(zKey);
    } catch (...) {
        return std::string();
    }
}

void Greeting::setTopBlock(const MiniNumber& zTopBlock) {
    mTopBlock = std::make_unique<MiniNumber>(zTopBlock);
}

const MiniNumber& Greeting::getTopBlock() const {
    return *mTopBlock;
}

const MiniString& Greeting::getVersion() const {
    return *mVersion;
}

MiniNumber Greeting::getRootBlock() const {
    if (mTopBlock->isEqual(MiniNumber::MINUSONE())) {
        return MiniNumber::MINUSONE();
    }

    // Check Upper Limit..
    if (mTopBlock->isMore(MiniNumber::TRILLION())) {
        // Something wrong here..
        MinimaLogger::log(std::string("[!] Greeting TopBlock error topblock:") + mTopBlock->toString()
                          + " ChainSize:" + std::to_string(mChain.size()));
        return MiniNumber::MINUSONE();
    }

    try {
        int offset = static_cast<int>(mChain.size()) - 1;
        MiniNumber rootblock = mTopBlock->sub(MiniNumber(offset));
        return rootblock;
    } catch (const std::exception& nfe) {
        MinimaLogger::log(nfe);
        MinimaLogger::log(std::string("Greeting calc root error.. topblock:")
                          + mTopBlock->toString()
                          + " ChainSize:" + std::to_string(mChain.size()));
        throw; // Rethrow as in Java
    }
}

std::vector<std::unique_ptr<MiniData>>& Greeting::getChain() {
    return mChain;
}

const std::vector<std::unique_ptr<MiniData>>& Greeting::getChain() const {
    return mChain;
}

void Greeting::writeDataStream(std::ostream& out) {
    // mVersion
    mVersion->writeDataStream(out);

    // JSON as a MiniString
    MiniString json(mExtraData->toString());
    json.writeDataStream(out);

    // Top block
    mTopBlock->writeDataStream(out);

    // Chain length and elements
    int len = static_cast<int>(mChain.size());
    MiniNumber::WriteToStream(out, len);
    for (const auto& txpowid : mChain) {
        txpowid->writeDataStream(out);
    }
}

void Greeting::readDataStream(std::istream& in) {
    // Version
    mVersion = std::make_unique<MiniString>(MiniString::ReadFromStream(in));

    // JSON MiniString, then parse
    MiniString json = MiniString::ReadFromStream(in);
    try {
        JSONParser parser;
        std::any parsed = parser.parse(json.toString());

        // Handle common return forms: shared_ptr<JSONObject> or JSONObject
        if (parsed.has_value()) {
            if (parsed.type() == typeid(std::shared_ptr<JSONObject>)) {
                auto jobj = std::any_cast<std::shared_ptr<JSONObject>>(parsed);
                if (jobj) {
                    mExtraData = std::make_unique<JSONObject>(*jobj);
                } else {
                    mExtraData = std::make_unique<JSONObject>();
                }
            } else if (parsed.type() == typeid(JSONObject)) {
                mExtraData = std::make_unique<JSONObject>(std::any_cast<JSONObject>(parsed));
            } else {
                // Fallback: empty object if unexpected type
                mExtraData = std::make_unique<JSONObject>();
            }
        } else {
            mExtraData = std::make_unique<JSONObject>();
        }
    } catch (const ParseException& e) {
        MinimaLogger::log(e);
        mExtraData = std::make_unique<JSONObject>();
    } catch (const std::exception& e) {
        // Defensive: other parsing exceptions
        MinimaLogger::log(e);
        mExtraData = std::make_unique<JSONObject>();
    }

    // Top block
    mTopBlock = std::make_unique<MiniNumber>(MiniNumber::ReadFromStream(in));

    // Chain
    mChain.clear();
    int len = MiniNumber::ReadFromStream(in).getAsInt();
    if (len < 0) {
        len = 0;
    }
    mChain.reserve(static_cast<std::size_t>(len));
    for (int i = 0; i < len; ++i) {
        MiniData md = MiniData::ReadFromStream(in);
        mChain.emplace_back(std::make_unique<MiniData>(md));
    }
}

std::shared_ptr<Greeting> Greeting::ReadFromStream(std::istream& in) {
    auto greet = std::make_shared<Greeting>();
    greet->readDataStream(in);
    return greet;
}

} // namespace objects
} // namespace minima
} // namespace org
