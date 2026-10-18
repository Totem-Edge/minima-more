#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/user_d_b.hpp" // For loadCustomTransactions/saveCustomTransactions
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/utils/minima_logger.hpp"

#ifdef _WIN32
// No platform-specific behavior required here; placeholder for future Windows specifics.
#endif

namespace org {
namespace minima {
namespace database {
namespace userprefs {
namespace txndb {

TxnDB::TxnDB() = default;

// Explicit special members due to unique_ptr to incomplete type (PIMPL fix)
TxnDB::~TxnDB() = default;
TxnDB::TxnDB(TxnDB&&) noexcept = default;
TxnDB& TxnDB::operator=(TxnDB&&) noexcept = default;

void TxnDB::loadDB() {
    using org::minima::database::MinimaDB;
    using org::minima::objects::base::MiniData;

    MiniData completedb = MinimaDB::getDB()->getUserDB().loadCustomTransactions();
    if (!completedb.isEqual(MiniData::ZERO_TXPOWID())) {
        auto txnDB = TxnDB::convertMiniDataVersion(completedb);
        if (txnDB) {
            mTransactions = std::move(txnDB->mTransactions);
        }
    }
}

void TxnDB::saveDB() {
    using org::minima::database::MinimaDB;
    using org::minima::objects::base::MiniData;

    auto md = MiniData::getMiniDataVersion(*this);
    if (md) {
        MinimaDB::getDB()->getUserDB().saveCustomTransactions(*md);
    }
}

void TxnDB::createTransaction(const std::string& zKey) {
    using org::minima::objects::Transaction;
    using org::minima::objects::Witness;

    auto tx = std::make_unique<Transaction>();
    auto wit = std::make_unique<Witness>();
    auto row = std::make_unique<org::minima::database::userprefs::txndb::TxnRow>(zKey, std::move(tx), std::move(wit));
    mTransactions.emplace_back(std::move(row));
}

void TxnDB::addCompleteTransaction(std::unique_ptr<org::minima::database::userprefs::txndb::TxnRow> zRow) {
    if (!zRow) {
        return;
    }
    // Remove any old one with same ID
    deleteTransaction(zRow->getID());
    // Add the new
    mTransactions.emplace_back(std::move(zRow));
}

org::minima::database::userprefs::txndb::TxnRow* TxnDB::getTransactionRow(const std::string& zKey) {
    for (auto& txn : mTransactions) {
        if (txn && txn->getID() == zKey) {
            return txn.get();
        }
    }
    return nullptr;
}

const org::minima::database::userprefs::txndb::TxnRow* TxnDB::getTransactionRow(const std::string& zKey) const {
    for (auto& txn : mTransactions) {
        if (txn && txn->getID() == zKey) {
            return txn.get();
        }
    }
    return nullptr;
}

bool TxnDB::deleteTransaction(const std::string& zKey) {
    bool found = false;
    std::vector<std::unique_ptr<org::minima::database::userprefs::txndb::TxnRow>> transactions;
    transactions.reserve(mTransactions.size());
    for (auto& txn : mTransactions) {
        if (txn && txn->getID() != zKey) {
            transactions.emplace_back(std::move(txn));
        } else {
            found = true;
        }
    }
    mTransactions = std::move(transactions);
    return found;
}

std::vector<std::unique_ptr<org::minima::database::userprefs::txndb::TxnRow>>& TxnDB::listTxns() {
    return mTransactions;
}

const std::vector<std::unique_ptr<org::minima::database::userprefs::txndb::TxnRow>>& TxnDB::listTxns() const {
    return mTransactions;
}

void TxnDB::clearTxns() {
    mTransactions.clear();
}

/**
 * Convert a MiniData version into a TxnDB
 */
std::unique_ptr<TxnDB> TxnDB::convertMiniDataVersion(const org::minima::objects::base::MiniData& zTxpData) {
    using org::minima::utils::MinimaLogger;

    const auto& bytes = zTxpData.getBytes();
    std::string buf(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::istringstream in(buf, std::ios::binary);

    std::unique_ptr<TxnDB> txnrow;
    try {
        txnrow = TxnDB::ReadFromStream(in);
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
    }
    return txnrow;
}

void TxnDB::writeDataStream(std::ostream& out) {
    using org::minima::objects::base::MiniNumber;

    // How many
    MiniNumber::WriteToStream(out, static_cast<int>(mTransactions.size()));
    for (auto& txnrow : mTransactions) {
        if (txnrow) {
            txnrow->writeDataStream(out);
        } else {
            // Java never stores null entries; skip if encountered.
        }
    }
}

void TxnDB::readDataStream(std::istream& in) {
    using org::minima::objects::base::MiniNumber;

    mTransactions.clear();
    int num = MiniNumber::ReadFromStream(in).getAsInt();
    mTransactions.reserve(static_cast<size_t>(std::max(0, num)));
    for (int i = 0; i < num; ++i) {
        // TxnRow::ReadFromStream returns a TxnRow by value in this codebase
        auto row_obj = org::minima::database::userprefs::txndb::TxnRow::ReadFromStream(in);
        mTransactions.emplace_back(std::make_unique<org::minima::database::userprefs::txndb::TxnRow>(std::move(row_obj)));
    }
}

std::unique_ptr<TxnDB> TxnDB::ReadFromStream(std::istream& in) {
    auto txp = std::make_unique<TxnDB>();
    txp->readDataStream(in);
    return txp;
}

} // namespace txndb
} // namespace userprefs
} // namespace database
} // namespace minima
} // namespace org