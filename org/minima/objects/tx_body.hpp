#pragma once

#include <memory>
#include <vector>
#include <string>

#include "org/minima/utils/streamable.hpp"

// Namespaced forward declarations to avoid include cycles (Pitfall 10)
namespace org { namespace minima { namespace objects { namespace base {
    class MiniData;
    class MiniNumber;
} } } }

namespace org { namespace minima { namespace objects {
    class Transaction;
    class Witness;
} } }

namespace org { namespace minima { namespace utils { namespace json {
    class JSONObject;
} } } }

namespace org {
namespace minima {
namespace objects {

class TxBody : public org::minima::utils::Streamable {
public:
    // Public members to mirror Java's public fields
    std::unique_ptr<org::minima::objects::base::MiniData> mPRNG;
    std::unique_ptr<org::minima::objects::base::MiniData> mTxnDifficulty;

    std::unique_ptr<org::minima::objects::Transaction> mTransaction;
    std::unique_ptr<org::minima::objects::Witness>     mWitness;

    std::unique_ptr<org::minima::objects::Transaction> mBurnTransaction;
    std::unique_ptr<org::minima::objects::Witness>     mBurnWitness;

    // List of current TXPOW IDs not yet in chain
    std::vector<std::unique_ptr<org::minima::objects::base::MiniData>> mTxPowIDList;

    // Constructor
    TxBody();

    // Destructor and move operations must be explicitly declared (Pitfall 1)
    virtual ~TxBody();
    TxBody(TxBody&&) noexcept;
    TxBody& operator=(TxBody&&) noexcept;

    // Delete copy operations
    TxBody(const TxBody& zOther);
    TxBody& operator=(const TxBody&) = delete;

    // Methods
    void resetRandomPRNG();

    // JSON conversion
    org::minima::utils::json::JSONObject toJSON() const;

    // Streamable overrides
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;
};

} // namespace objects
} // namespace minima
} // namespace org