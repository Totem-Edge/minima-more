#include "org/minima/database/userprefs/txndb/txn_row.hpp"

#include <sstream>
#include <vector>

#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/coin.hpp" // Required so the compiler knows Coin derives from Streamable
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/system/brains/tx_po_w_generator.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

#ifdef _WIN32
// No OS-specific behavior required currently; placeholder for future needs.
#endif

namespace org {
namespace minima {
namespace database {
namespace userprefs {
namespace txndb {

// Destructor and move operations definitions (PITFALL 1)
TxnRow::~TxnRow() = default;
TxnRow::TxnRow(TxnRow&&) noexcept = default;
TxnRow& TxnRow::operator=(TxnRow&&) noexcept = default;

TxnRow::TxnRow() = default;

TxnRow::TxnRow(const std::string& zID,
               std::unique_ptr<org::minima::objects::Transaction> zTransaction,
               std::unique_ptr<org::minima::objects::Witness> zWitness)
    : mID(zID),
      mTransaction(std::move(zTransaction)),
      mWitness(std::move(zWitness)) {
}

void TxnRow::setID(const std::string& zID) {
    mID = zID;
}

std::string TxnRow::getID() const {
    return mID;
}

org::minima::objects::Transaction& TxnRow::getTransaction() {
    return *mTransaction;
}

const org::minima::objects::Transaction& TxnRow::getTransaction() const {
    return *mTransaction;
}

org::minima::objects::Witness& TxnRow::getWitness() {
    return *mWitness;
}

const org::minima::objects::Witness& TxnRow::getWitness() const {
    return *mWitness;
}

void TxnRow::clearWitness() {
    mWitness = std::make_unique<org::minima::objects::Witness>();
}

org::minima::utils::json::JSONObject TxnRow::toJSON() {
    return toJSON(true);
}

org::minima::utils::json::JSONObject TxnRow::toJSON(bool zShowWitness) {
    using org::minima::utils::json::JSONObject;
    using org::minima::utils::json::JSONArray;
    using org::minima::objects::base::MiniData;
    using org::minima::system::brains::TxPoWGenerator;

    JSONObject ret;
    ret.put("id", mID);

    // Calculate the correct CoinID - if possible..
    if (mTransaction) {
        TxPoWGenerator::precomputeTransactionCoinID(*mTransaction);
        ret.put("transaction", mTransaction->toJSON());
    } else {
        // Java would NPE; to avoid UB, emit empty object
        ret.put("transaction", JSONObject());
    }

    if (zShowWitness) {
        if (mWitness) {
            ret.put("witness", mWitness->toJSON());
        } else {
            ret.put("witness", JSONObject());
        }

        // Now output the full output coins with correct coinid
        JSONArray coinarr;
        if (mTransaction) {
            auto& outs = mTransaction->getAllOutputs();
            for (const auto& cc : outs) {
                if (!cc) continue;
                auto md = MiniData::getMiniDataVersion(*cc);
                if (md) {
                    coinarr.add(md->to0xString());
                }
            }
        }
        ret.put("outputcoindata", coinarr);
    }

    return ret;
}

std::unique_ptr<TxnRow> TxnRow::convertMiniDataVersion(const org::minima::objects::base::MiniData& zTxpData) {
    using org::minima::utils::MinimaLogger;

    try {
        const std::vector<std::uint8_t>& bytes = zTxpData.getBytes();
        std::string buf(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        std::istringstream in(buf);

        TxnRow row = TxnRow::ReadFromStream(in);
        return std::make_unique<TxnRow>(std::move(row));
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return nullptr;
    }
}

void TxnRow::writeDataStream(std::ostream& zOut) {
    using org::minima::objects::base::MiniString;

    // ID
    MiniString::WriteToStream(zOut, mID);

    // Transaction and Witness
    if (!mTransaction) {
        // Maintain behavior similar to Java NPE by throwing if null
        throw std::runtime_error("TxnRow::writeDataStream - mTransaction is null");
    }
    if (!mWitness) {
        throw std::runtime_error("TxnRow::writeDataStream - mWitness is null");
    }

    mTransaction->writeDataStream(zOut);
    mWitness->writeDataStream(zOut);
}

void TxnRow::readDataStream(std::istream& zIn) {
    using org::minima::objects::base::MiniString;
    using org::minima::objects::Transaction;
    using org::minima::objects::Witness;

    // Read ID
    mID = MiniString::ReadFromStream(zIn).toString();

    // Read Transaction
    mTransaction = std::make_unique<Transaction>();
    mTransaction->readDataStream(zIn);

    // Read Witness
    mWitness = std::make_unique<Witness>();
    mWitness->readDataStream(zIn);
}

TxnRow TxnRow::ReadFromStream(std::istream& zIn) {
    TxnRow txp;
    txp.readDataStream(zIn);
    return txp;
}

} // namespace txndb
} // namespace userprefs
} // namespace database
} // namespace minima
} // namespace org