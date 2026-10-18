#include "org/minima/system/commands/base/consolidate.hpp"

#include <any>
#include <sstream>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/system/commands/send/send.hpp"

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/wallet/script_row.hpp"

#include "org/minima/system/brains/tx_po_w_searcher.hpp"
#include "org/minima/system/params/global_params.hpp"

#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::objects::Coin;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::system::brains::TxPoWSearcher;
using org::minima::utils::MinimaLogger;
using org::minima::utils::json::JSONObject;

consolidate::consolidate()
: org::minima::system::commands::Command(
      "consolidate",
      "[tokenid:] (coinage:) (maxcoins:) (maxsigs:) (burn:) (debug:) (dryrun:) - Consolidate coins by sending them back to yourself") {
}

std::string consolidate::getFullHelp() const {
    return
        "\nconsolidate\n"
        "\n"
        "Consolidate multiple coins (UTxOs) into one by sending them back to yourself. Must have at least 3 coins.\n"
        "\n"
        "Useful to prevent having many coins of tiny value and to manage the number of coins you are tracking.\n"
        "\n"
        "Optionally set the minimum coin age (in blocks), maximum number of coins and maximum number of signatures for the transaction.\n"
        "\n"
        "tokenid:\n"
        "    The tokenid for Minima or custom token to consolidate coins for. Minima is 0x00.\n"
        "\n"
        "coinage: (optional)\n"
        "    The minimum number of blocks deep (confirmations) a coin needs to be. Default is 3.\n"
        "\n"
        "maxcoins: (optional)\n"
        "    The maximum number of coins to consolidate. Minimum 3, up to 20.\n"
        "    Coins are first sorted by value (smallest first) before adding to the transaction.\n"
        "\n"
        "maxsigs: (optional)\n"
        "    The maximum number of signatures for the transaction, up to 5.\n"
        "    Coins are then sorted by address to minimize the number of signatures required.\n"
        "\n"
        "burn: (optional)\n"
        "    Amount of Minima to burn with the transaction.\n"
        "\n"
        "debug: (optional)\n"
        "    true or false, true will print more detailed logs.\n"
        "\n"
        "dryrun: (optional)\n"
        "    true or false, true will simulate the consolidate transaction but not execute it.\n"
        "\n"
        "password: (optional)\n"
        "    If your Wallet is password locked you can unlock it for this one transaction - then relock it.\n "
        "\n"
        "Examples:\n"
        "\n"
        "consolidate tokenid:0x00\n"
        "\n"
        "consolidate tokenid:0x77.. coinage:10\n"
        "\n"
        "consolidate tokenid:0x00 maxcoins:5 password:your_password\n"
        "\n"
        "consolidate tokenid:0x00 coinage:10 maxcoins:8 burn:1\n"
        "\n"
        "consolidate tokenid:0x00 coinage:10 maxcoins:8 maxsigs:3 burn:1 dryrun:true\n";
}

std::vector<std::string> consolidate::getValidParams() const {
    return std::vector<std::string>{
        "tokenid","coinage","maxcoins","maxsigs","burn","debug","dryrun","password"
    };
}

std::unique_ptr<JSONObject> consolidate::runCommand() {
    // JSON reply
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // The tokenid (required)
    const std::string tokenid = getParam("tokenid");

    // Burn amount (default ZERO)
    std::unique_ptr<MiniNumber> burnptr = getNumberParam("burn", MiniNumber::ZERO());
    MiniNumber burn = *burnptr;

    // Debug / dryrun
    bool debug  = getBooleanParam("debug", false);
    bool dryrun = getBooleanParam("dryrun", false);
    if (dryrun) {
        debug = true;
    }

    // Tip of the tree
    auto txptree = org::minima::database::MinimaDB::getDB()->getTxPoWTree();
    auto tip = txptree.getTip();

    // Coinage minimum (default GlobalParams.MINIMA_CONFIRM_DEPTH)
    std::unique_ptr<MiniNumber> coinageptr = getNumberParam(
        "coinage", org::minima::system::params::GlobalParams::MINIMA_CONFIRM_DEPTH);
    MiniNumber coinage = *coinageptr;

    if (coinage.isLess(org::minima::system::params::GlobalParams::MINIMA_CONFIRM_DEPTH)) {
        throw org::minima::system::commands::CommandException(
            "Coinage MUST be >= " + org::minima::system::params::GlobalParams::MINIMA_CONFIRM_DEPTH.toString());
    }

    // Get relevant unspent coins for tokenid (simple only = true)
    std::vector<std::shared_ptr<Coin>> foundcoins =
        TxPoWSearcher::getRelevantUnspentCoins(tip, tokenid, true);

    std::vector<std::shared_ptr<Coin>> relcoins;
    relcoins.reserve(foundcoins.size());

    // Ensure coins are old enough
    MiniNumber mincoinblock = tip->getBlockNumber().sub(coinage);
    for (const auto& relc : foundcoins) {
        if (relc && relc->getBlockCreated().isLessEqual(mincoinblock)) {
            relcoins.push_back(relc);
        }
    }

    // Convert to unique_ptr<Coin> deep copies to use send::orderCoins
    std::vector<std::unique_ptr<Coin>> relcoins_unique;
    relcoins_unique.reserve(relcoins.size());
    for (const auto& c : relcoins) {
        if (c) {
            auto dup = c->deepCopy();
            if (dup) {
                relcoins_unique.push_back(std::move(dup));
            }
        }
    }

    // Sort coins via same address - to minimize signatures
    std::vector<std::shared_ptr<org::minima::objects::Coin>> relcoins_shared;
    for (auto& coin : relcoins_unique) {
        relcoins_shared.push_back(std::move(coin));
    }
    auto ordered = org::minima::system::commands::send::send::orderCoins(relcoins_shared);

    // How many coins are there
    int totcoins = static_cast<int>(ordered.size());
    if (totcoins < 3) {
        throw org::minima::system::commands::CommandException(
            "Not enough coins (" + std::to_string(totcoins) + ") to consolidate");
    }

    // Maximum number of coins and signatures
    int MAX_SIGS  = getNumberParam("maxsigs", MiniNumber(5))->getAsInt();
    int MAX_COINS = getNumberParam("maxcoins", MiniNumber(20))->getAsInt();

    std::string currentaddress;
    MiniNumber  totalamount = MiniNumber::ZERO();
    int         totalsigs   = 0;
    int         totalcoins  = 0;

    for (const auto& cptr : ordered) {
        const Coin& cc = *cptr;

        // Coin address
        std::string coinaddress = cc.getAddress().to0xString();

        // Coin amount (scaled for tokens)
        MiniNumber coinamount = cc.getAmount();
        if (cc.getTokenID().to0xString() != "0x00") {
            coinamount = cc.getTokenAmount();
        }

        // New address => new signature
        if (currentaddress != coinaddress) {
            if ((totalsigs + 1) > MAX_SIGS) {
                if (debug) {
                    MinimaLogger::log("Consolidate - max sigs reached " + std::to_string(totalsigs));
                }
                break;
            }
            currentaddress = coinaddress;
            totalsigs++;
        }

        // Add to total
        totalamount = totalamount.add(coinamount);

        // One more coin
        totalcoins++;

        if (debug) {
            MinimaLogger::log(
                std::string("Consolidate - add coin ") + coinamount.toString() +
                " totalcoins:" + std::to_string(totalcoins) +
                "  totalsigs:" + std::to_string(totalsigs) +
                " coinid:" + cc.getCoinID().to0xString());
        }

        if (totalcoins >= MAX_COINS) {
            if (debug) {
                MinimaLogger::log("Consolidate - max coins reached " + std::to_string(totalcoins));
            }
            break;
        }
    }

    // Get one of your addresses (default)
    auto newwalletaddress = org::minima::database::MinimaDB::getDB()->getWallet().getDefaultAddress();
    MiniData myaddress(newwalletaddress->getAddress());

    // Construct the command
    auto boolToString = [](bool b) -> std::string { return b ? "true" : "false"; };

    std::string command =
        std::string("send coinage:") + coinage.toString() +
        " split:2 dryrun:" + boolToString(dryrun) +
        " debug:" + boolToString(debug) +
        " burn:" + burn.toString() +
        " amount:" + totalamount.toString() +
        " address:" + myaddress.to0xString() +
        " tokenid:" + tokenid;

    // Password (optional)
    if (existsParam("password")) {
        if (debug) {
            MinimaLogger::log(std::string("Consolidate command : ") + command + " password:####");
        }
        command += " password:\"";
        command += getParam("password");
        command += "\"";
    } else {
        if (debug) {
            MinimaLogger::log(std::string("Consolidate command : ") + command);
        }
    }

    // Execute the send command (single)
    std::shared_ptr<JSONObject> sendresult =
        org::minima::system::commands::CommandRunner::getRunner()->runSingleCommand(command);

    if (sendresult && sendresult->getBoolean("status")) {
        // Success
        ret->put("response", sendresult->get("response"));
    } else {
        // Failure path - propagate message or error
        ret->put("status", false);
        if (sendresult && sendresult->containsKey("message")) {
            ret->put("message", sendresult->get("message"));
        } else if (sendresult && sendresult->containsKey("error")) {
            ret->put("message", sendresult->get("error"));
        } else {
            // Put the whole object if no clear message
            ret->put("message", sendresult);
        }
    }

    return ret;
}

org::minima::system::commands::Command* consolidate::getFunction() {
    return new consolidate();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org