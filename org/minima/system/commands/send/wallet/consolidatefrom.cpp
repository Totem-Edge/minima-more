#include "org/minima/system/commands/send/wallet/consolidatefrom.hpp"

#include <algorithm>
#include <limits>
#include <sstream>
#include <memory>
#include <any>
#include <typeinfo>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/commands/command_runner.hpp"

#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"

#include "org/minima/system/brains/tx_po_w_searcher.hpp"

#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include "org/minima/system/params/general_params.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {
namespace wallet {

using org::minima::objects::Coin;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::utils::json::JSONObject;

consolidatefrom::consolidatefrom()
    : org::minima::system::commands::Command(
          "consolidatefrom",
          "[fromaddress:] [address:] [amount:] (tokenid:) [script:] [privatekey:] [keyuses:] (burn:) (mine:) - Send Minima or Tokens from a certain address") {}

std::vector<std::string> consolidatefrom::getValidParams() const {
    return {
        "fromaddress",
        "tokenid",
        "script",
        "privatekey",
        "keyuses",
        "mine",
        "burn",
        "maxcoins"
    };
}

std::unique_ptr<JSONObject> consolidatefrom::runCommand() {
    auto ret = getJSONReply();

    // From which address
    std::string fromaddress = getAddressParam("fromaddress");
    std::string tokenid     = getAddressParam("tokenid", "0x00");

    // How many coins to add
    int maxcoins = getNumberParam("maxcoins", MiniNumber(50))->getAsInt();

    // Get the BURN
    MiniNumber burn = *getNumberParam("burn", MiniNumber::ZERO());
    if (burn.isMore(MiniNumber::ZERO()) && tokenid != "0x00") {
        throw org::minima::system::commands::CommandException(
            "Currently BURN only works for Minima.. tokenid:0x00.. not tokens.");
    }

    // The script of the address
    std::string script = getParam("script");

    // The private key we need to sign with
    std::string privatekey = getAddressParam("privatekey");
    MiniNumber keyuses     = *getNumberParam("keyuses");

    // ID of the custom transaction
    std::string randomid = MiniData::getRandomData(32).to0xString();

    // Now construct the transaction..
    std::shared_ptr<JSONObject> result = runCommand("txncreate id:" + randomid);

    // Are we mining
    bool mine = getBooleanParam("mine", true);

    // Get chain tip and current block number
    auto tip = org::minima::database::MinimaDB::getDB()->getTxPoWTree().getTip();
    auto currentblock = tip->getBlockNumber();

    // Get all the coins for this address / token
    std::vector<std::shared_ptr<Coin>> coins = org::minima::system::brains::TxPoWSearcher::searchCoins(
        tip,
        false,                                      // zRelevant
        false, MiniData::ZERO_TXPOWID(),              // zCheckCoinID, zCoinID
        false, MiniNumber::ZERO(),                    // zCheckAmount, zAmount
        true, MiniData(fromaddress),                // zCheckAddress, zAddress
        true, MiniData(tokenid),                    // zCheckTokenID, zTokenID
        false, std::string(""), true,               // zCheckState, zState, zWildCardState
        false, std::numeric_limits<int>::max(),     // zSimpleOnly, zDepth
        org::minima::system::params::GeneralParams::IS_MEGAMMR // zMEGAMMR
    );

    // Now order the coins by amount descending
    std::sort(coins.begin(), coins.end(),
              [](const std::shared_ptr<Coin>& a, const std::shared_ptr<Coin>& b) {
                  MiniNumber amt1 = a->getAmount();
                  MiniNumber amt2 = b->getAmount();
                  return amt2.isMore(amt1);
              });

    // Now make sure they are old enough and not in mempool
    org::minima::database::txpowdb::TxPoWDB& txpdb =
        org::minima::database::MinimaDB::getDB()->getTxPoWDB();
    MiniNumber minage(3);
    std::vector<std::shared_ptr<Coin>> validcoins;
    validcoins.reserve(coins.size());

    for (const auto& cc : coins) {
        bool stillgood = true;

        // Check age
        if (currentblock.sub(cc->getBlockCreated()).isLess(minage)) {
            stillgood = false;
        }

        // Check mempool
        if (stillgood) {
            if (txpdb.checkMempoolCoins(cc->getCoinID())) {
                stillgood = false;
            }
        }

        if (stillgood) {
            validcoins.push_back(cc);
        }
    }

    if (validcoins.empty()) {
        std::string msg = "No valid coins found to consolidate. Coins must be " +
                          minage.toString() + " blocks old and not already in mempool.";
        throw org::minima::system::commands::CommandException(msg);
    }

    // Now add up to maxcoins coins
    int coincount = 1;
    MiniNumber total = MiniNumber::ZERO();

    for (const auto& cc : validcoins) {
        total = total.add(cc->getTokenAmount());

        // Add this coin to the transaction
        std::string cmd = "txninput id:" + randomid + " coinid:" + cc->getCoinID().to0xString();
        result = runCommand(cmd);

        // How many coins have been added
        coincount++;
        if (coincount > maxcoins) {
            break;
        }
    }

    // Now do the burn..
    if (tokenid == "0x00" && burn.isMore(MiniNumber::ZERO())) {
        if (burn.isLess(total)) {
            total = total.sub(burn);
        } else {
            // Remove the txn..
            runCommand("txndelete id:" + randomid);

            std::string err = "Burn greater than total amount added " + burn.toString()
                            + " / " + total.toString();
            throw org::minima::system::commands::CommandException(err);
        }
    }

    // And now add the outputs - 10 outputs
    MiniNumber smallout = total.div(MiniNumber(10));
    for (int i = 0; i < 10; ++i) {
        std::string outcmd = "txnoutput id:" + randomid
                           + " amount:" + smallout.toString()
                           + " address:" + fromaddress
                           + " tokenid:" + tokenid;
        result = runCommand(outcmd);
    }

    // Add the scripts..
    {
        std::string scmd = "txnscript id:" + randomid + " scripts:{\"" + script + "\":\"\"}";
        runCommand(scmd);
    }

    // Sort the MMR
    runCommand("txnmmr id:" + randomid);

    // Now SIGN
    {
        std::string signcmd = "txnsign id:" + randomid
                            + " publickey:custom"
                            + " privatekey:" + privatekey
                            + " keyuses:" + keyuses.toString();
        runCommand(signcmd);
    }

    // And POST!
    {
        std::string postcmd = "txnpost id:" + randomid + " mine:" + std::string(mine ? "true" : "false");
        result = runCommand(postcmd);
    }

    // And delete..
    runCommand("txndelete id:" + randomid);

    // And return..
    ret->put("response", result->get("response"));

    return ret;
}

std::shared_ptr<JSONObject> consolidatefrom::createConsolidate(
    const std::vector<std::shared_ptr<Coin>>& zAllCoins,
    const MiniNumber& zBurn,
    const std::string& zFromAddress,
    const std::string& zTokenid,
    const std::string& zScript,
    const std::string& zPrivateKey,
    const MiniNumber& zKeyUses) {

    // ID of the custom transaction
    std::string randomid = MiniData::getRandomData(32).to0xString();

    // Now construct the transaction..
    std::shared_ptr<JSONObject> result = runCommand("txncreate id:" + randomid);

    // Now add ALL these coins
    MiniNumber total = MiniNumber::ZERO();

    for (const auto& cc : zAllCoins) {
        total = total.add(cc->getTokenAmount());
        // Add this coin to the transaction
        runCommand("txninput id:" + randomid + " coinid:" + cc->getCoinID().to0xString());
    }

    // Now do the burn..
    if (zBurn.isMore(MiniNumber::ZERO())) {
        if (zBurn.isLess(total)) {
            total = total.sub(zBurn);
        } else {
            // Remove the txn..
            runCommand("txndelete id:" + randomid);

            std::string err = "Burn greater than total amount added " + zBurn.toString()
                            + " / " + total.toString();
            throw org::minima::system::commands::CommandException(err);
        }
    }

    // And now add the output
    {
        std::string outcmd = "txnoutput id:" + randomid
                           + " amount:" + total.toString()
                           + " address:" + zFromAddress
                           + " tokenid:" + zTokenid;
        result = runCommand(outcmd);
    }

    // Add the scripts..
    runCommand("txnscript id:" + randomid + " scripts:{\"" + zScript + "\":\"\"}");

    // Sort the MMR
    runCommand("txnmmr id:" + randomid);

    // Now SIGN
    {
        std::string signcmd = "txnsign id:" + randomid
                            + " publickey:custom"
                            + " privatekey:" + zPrivateKey
                            + " keyuses:" + zKeyUses.toString();
        runCommand(signcmd);
    }

    // And POST!
    result = runCommand("txnpost id:" + randomid + " mine:true");

    // And delete..
    runCommand("txndelete id:" + randomid);

    // Return the POST response
    try {
        const std::any& resp_any = result->get("response");
        if (resp_any.type() == typeid(std::shared_ptr<JSONObject>)) {
            return std::any_cast<std::shared_ptr<JSONObject>>(resp_any);
        }
        if (resp_any.type() == typeid(JSONObject)) {
            return std::make_shared<JSONObject>(std::any_cast<JSONObject>(resp_any));
        }
    } catch (...) {
        // ignore and fall back
    }

    // Fallback: return the whole result if response isn't a JSONObject
    return result;
}

std::shared_ptr<JSONObject> consolidatefrom::runCommand(const std::string& zCommand) {
    // Use single-command runner which returns the first JSON object result
    return org::minima::system::commands::CommandRunner::getRunner()->runSingleCommand(zCommand);
}

org::minima::system::commands::Command* consolidatefrom::getFunction() {
    return new consolidatefrom();
}

} // namespace wallet
} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org