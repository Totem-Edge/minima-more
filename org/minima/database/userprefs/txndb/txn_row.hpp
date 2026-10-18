#pragma once

#include <memory>
#include <string>
#include <istream>
#include <ostream>

#include "org/minima/utils/streamable.hpp"

// Namespaced forward declarations to avoid heavy includes in header (PITFALL 4)
namespace org { namespace minima { namespace objects { class Transaction; } } }
namespace org { namespace minima { namespace objects { class Witness; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; } } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace database {
namespace userprefs {
namespace txndb {

class TxnRow : public org::minima::utils::Streamable {
public:
    // Destructor and move operations for unique_ptr to incomplete types (PITFALL 1)
    virtual ~TxnRow();
    TxnRow(TxnRow&&) noexcept;
    TxnRow& operator=(TxnRow&&) noexcept;

    // Delete copy ops
    TxnRow(const TxnRow&) = delete;
    TxnRow& operator=(const TxnRow&) = delete;

    // Constructor taking ownership of Transaction and Witness
    TxnRow(const std::string& zID,
           std::unique_ptr<org::minima::objects::Transaction> zTransaction,
           std::unique_ptr<org::minima::objects::Witness> zWitness);

    // ID accessors
    void setID(const std::string& zID);
    std::string getID() const;

    // Accessors for contained objects
    org::minima::objects::Transaction& getTransaction();
    const org::minima::objects::Transaction& getTransaction() const;

    org::minima::objects::Witness& getWitness();
    const org::minima::objects::Witness& getWitness() const;

    // Clear witness (replace with a new empty Witness)
    void clearWitness();

    // JSON
    org::minima::utils::json::JSONObject toJSON();
    org::minima::utils::json::JSONObject toJSON(bool zShowWitness);

    // Convert a MiniData blob into a TxnRow (returns nullptr on error)
    static std::unique_ptr<TxnRow> convertMiniDataVersion(const org::minima::objects::base::MiniData& zTxpData);

    // Streamable
    void writeDataStream(std::ostream& zOut) override;
    void readDataStream(std::istream& zIn) override;

    // Static read helper (Java-style)
    static TxnRow ReadFromStream(std::istream& zIn);

private:
    // Private default constructor (used by ReadFromStream)
    TxnRow();

public:
    // Keep public members to mimic Java's public fields for functional parity
    std::string mID;
    std::unique_ptr<org::minima::objects::Transaction> mTransaction;
    std::unique_ptr<org::minima::objects::Witness>     mWitness;
};

} // namespace txndb
} // namespace userprefs
} // namespace database
} // namespace minima
} // namespace org