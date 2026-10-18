#pragma once

#include <memory>
#include <vector>
#include <string>
#include <ostream>
#include <istream>

#include "org/minima/utils/streamable.hpp"

// Namespaced forward declarations (avoid heavy includes in header)
namespace org { namespace minima { namespace database { namespace userprefs { namespace txndb { class TxnRow; } } } } }
namespace org { namespace minima { namespace objects { class Transaction; } } }
namespace org { namespace minima { namespace objects { class Witness; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; } } } }

namespace org {
namespace minima {
namespace database {
namespace userprefs {
namespace txndb {

class TxnDB : public org::minima::utils::Streamable {
public:
    TxnDB();

    // PIMPL-fix: unique_ptr to incomplete type requires explicit special members
    virtual ~TxnDB();
    TxnDB(TxnDB&&) noexcept;
    TxnDB& operator=(TxnDB&&) noexcept;

    // Delete copy operations (owning unique_ptr members)
    TxnDB(const TxnDB&) = delete;
    TxnDB& operator=(const TxnDB&) = delete;

    // DB load/save (delegates to MinimaDB UserDB like Java)
    void loadDB();
    void saveDB();

    // Create a blank transaction row for a given key
    void createTransaction(const std::string& zKey);

    // Add a fully constructed transaction row (removes any existing with same ID)
    void addCompleteTransaction(std::unique_ptr<org::minima::database::userprefs::txndb::TxnRow> zRow);

    // Find a transaction row by key; returns nullptr if not found
    org::minima::database::userprefs::txndb::TxnRow* getTransactionRow(const std::string& zKey);
    const org::minima::database::userprefs::txndb::TxnRow* getTransactionRow(const std::string& zKey) const;

    // Delete by key; returns true if found and removed
    bool deleteTransaction(const std::string& zKey);

    // List all transactions
    std::vector<std::unique_ptr<org::minima::database::userprefs::txndb::TxnRow>>& listTxns();
    const std::vector<std::unique_ptr<org::minima::database::userprefs::txndb::TxnRow>>& listTxns() const;

    // Clear all
    void clearTxns();

    // Convert MiniData -> TxnDB (static helper)
    static std::unique_ptr<TxnDB> convertMiniDataVersion(const org::minima::objects::base::MiniData& zTxpData);

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Static read helper (like Java ReadFromStream)
    static std::unique_ptr<TxnDB> ReadFromStream(std::istream& in);

public:
    // Public like Java
    std::vector<std::unique_ptr<org::minima::database::userprefs::txndb::TxnRow>> mTransactions;
};

} // namespace txndb
} // namespace userprefs
} // namespace database
} // namespace minima
} // namespace org