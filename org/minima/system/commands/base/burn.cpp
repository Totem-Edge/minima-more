#include "org/minima/system/commands/base/burn.hpp"

#include <algorithm>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::objects::TxPoW;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::utils::json::JSONObject;
using org::minima::database::MinimaDB;
using org::minima::database::txpowdb::TxPoWDB;
using org::minima::database::txpowtree::TxPoWTreeNode;

burn::burn()
    : org::minima::system::commands::Command("burn", "View Burn metrics"),
      mMinburn(MiniNumber::BILLION()),
      mMaxburn(MiniNumber::ZERO()),
      mBurnTot(MiniNumber::ZERO()),
      mBurnCount(MiniNumber::ZERO()),
      mValues(),
      mMinburn10(MiniNumber::BILLION()),
      mMaxburn10(MiniNumber::ZERO()),
      mBurnTot10(MiniNumber::ZERO()),
      mBurnCount10(MiniNumber::ZERO()),
      mValues10(),
      mMinburn50(MiniNumber::BILLION()),
      mMaxburn50(MiniNumber::ZERO()),
      mBurnTot50(MiniNumber::ZERO()),
      mBurnCount50(MiniNumber::ZERO()),
      mValues50() {
}

std::string burn::getFullHelp() const {
    return "\nburn\n"
           "\n"
           "View the number of burn transactions and the maximum, median, average and minimum burn metrics for the last 1, 10 and 50 blocks.\n"
           "\n"
           "Use as an indicator for an appropriate burn amount for transactions.\n"
           "\n"
           "Examples:\n"
           "\n"
           "burn\n";
}

std::unique_ptr<JSONObject> burn::runCommand() {
    auto ret = getJSONReply();

    std::string action = getParam("action", "list");

    JSONObject response;

    TxPoWDB& txpdb = MinimaDB::getDB()->getTxPoWDB();

    if (action == "list") {
        // Get the tip
        auto tip = MinimaDB::getDB()->getTxPoWTree().getTip();

        int counter = 0;
        while (tip && counter < 50) {
            // Get the block TxPoW
            const TxPoW& tiptxpow = tip->getTxPoW();

            // Adjust burn values for the block
            checkBurn(tiptxpow, counter);

            // All transactions in the block
            std::vector<MiniData> txns = tiptxpow.getBlockTransactions();
            for (const MiniData& txn : txns) {
                // Get the Transaction TxPoW by ID
                std::shared_ptr<TxPoW> txp = txpdb.getTxPoW(txn.to0xString());
                // Adjust Burn values if we have the txn
                if (txp) {
                    checkBurn(*txp, counter);
                }
            }
            

            // Move to parent
            tip = tip->getParent();
            ++counter;
        }

        if (mBurnCount.isEqual(MiniNumber::ZERO())) {
            mMinburn = MiniNumber::ZERO();
        }
        if (mBurnCount10.isEqual(MiniNumber::ZERO())) {
            mMinburn10 = MiniNumber::ZERO();
        }
        if (mBurnCount50.isEqual(MiniNumber::ZERO())) {
            mMinburn50 = MiniNumber::ZERO();
        }

        // Build JSON blocks
        JSONObject block1;
        block1.put("txns", mBurnCount);
        block1.put("max", mMaxburn);
        block1.put("med", getMediaValue(mValues));
        if (mBurnCount.isMore(MiniNumber::ZERO())) {
            block1.put("avg", mBurnTot.div(mBurnCount));
        } else {
            block1.put("avg", MiniNumber::ZERO());
        }
        block1.put("min", mMinburn);

        JSONObject block10;
        block10.put("txns", mBurnCount10);
        block10.put("max", mMaxburn10);
        block10.put("med", getMediaValue(mValues10));
        if (mBurnCount10.isMore(MiniNumber::ZERO())) {
            block10.put("avg", mBurnTot10.div(mBurnCount10));
        } else {
            block10.put("avg", MiniNumber::ZERO());
        }
        block10.put("min", mMinburn10);

        JSONObject block50;
        block50.put("txns", mBurnCount50);
        block50.put("max", mMaxburn50);
        block50.put("med", getMediaValue(mValues50));
        if (mBurnCount50.isMore(MiniNumber::ZERO())) {
            block50.put("avg", mBurnTot50.div(mBurnCount50));
        } else {
            block50.put("avg", MiniNumber::ZERO());
        }
        block50.put("min", mMinburn50);

        response.put("1block", block1);
        response.put("10block", block10);
        response.put("50block", block50);
    }

    ret->put("response", response);
    return ret;
}

void burn::checkBurn(const TxPoW& zTxPoW, int counter) {
    if (!zTxPoW.isTransaction()) {
        return;
    }

    MiniNumber burn = zTxPoW.getBurn();

    if (counter == 0) {
        if (burn.isMore(mMaxburn)) {
            mMaxburn = burn;
        }
        if (burn.isLess(mMinburn)) {
            mMinburn = burn;
        }

        mBurnCount = mBurnCount.increment();
        mBurnTot = mBurnTot.add(burn);
        mValues.push_back(burn);
    }

    // Total for last 10
    if (counter < 10) {
        if (burn.isMore(mMaxburn10)) {
            mMaxburn10 = burn;
        }
        if (burn.isLess(mMinburn10)) {
            mMinburn10 = burn;
        }

        mBurnCount10 = mBurnCount10.increment();
        mBurnTot10 = mBurnTot10.add(burn);
        mValues10.push_back(burn);
    }

    // And the last 50
    if (burn.isMore(mMaxburn50)) {
        mMaxburn50 = burn;
    }
    if (burn.isLess(mMinburn50)) {
        mMinburn50 = burn;
    }

    mBurnCount50 = mBurnCount50.increment();
    mBurnTot50 = mBurnTot50.add(burn);
    mValues50.push_back(burn);
}

MiniNumber burn::getMediaValue(std::vector<MiniNumber>& zValues) {
    if (zValues.empty()) {
        return MiniNumber::ZERO();
    }

    // Sort descending: Java comparator used o2.compareTo(o1)
    std::sort(zValues.begin(), zValues.end(),
              [](const MiniNumber& a, const MiniNumber& b) {
                  return a.isMore(b); // a before b if a > b (descending)
              });

    std::size_t size = zValues.size();
    return zValues[size / 2];
}

org::minima::system::commands::Command* burn::getFunction() {
    return new burn();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org