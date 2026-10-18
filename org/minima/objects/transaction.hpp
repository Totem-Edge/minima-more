#pragma once

#include <memory> // <--- ADDED
#include <vector>
#include <string>

#include "org/minima/utils/streamable.hpp"
#include "org/minima/objects/coin.hpp"

// Namespaced forward declarations (avoid including heavy headers in .hpp)
namespace org { namespace minima { namespace objects { class Coin; } } }
namespace org { namespace minima { namespace objects { class StateVariable; } } }
namespace org { namespace minima { namespace objects { class Token; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; } } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniNumber; } } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONArray; } } } }

namespace org {
namespace minima {
namespace objects {

class Transaction : public org::minima::utils::Streamable {
public:
    // Constructor/Destructor
    Transaction();
    virtual ~Transaction();                  // explicit for unique_ptr to incomplete types

    // Move operations (explicit for unique_ptr to incomplete types)
    Transaction(Transaction&&) noexcept;
    Transaction& operator=(Transaction&&) noexcept;

    // Delete copy operations
    Transaction(const Transaction& zOther);
    Transaction& operator=(const Transaction&) = delete;

    // Inputs / Outputs management
    void addInput(std::shared_ptr<org::minima::objects::Coin> zCoin); // <--- CHANGED
    void addOutput(std::unique_ptr<org::minima::objects::Coin> zCoin);

    bool isEmpty() const;

    // Accessors to internal collections (modifiable and const)
    std::vector<std::shared_ptr<org::minima::objects::Coin>>& getAllInputs(); // <--- CHANGED
    const std::vector<std::shared_ptr<org::minima::objects::Coin>>& getAllInputs() const; // <--- CHANGED

    std::vector<std::unique_ptr<org::minima::objects::Coin>>& getAllOutputs();
    const std::vector<std::unique_ptr<org::minima::objects::Coin>>& getAllOutputs() const;

    // Monotonic flags
    bool isCheckedMonotonic() const;
    void clearIsMonotonic();

    // Summations
    org::minima::objects::base::MiniNumber sumInputs();
    org::minima::objects::base::MiniNumber sumInputs(const org::minima::objects::base::MiniData& zTokenID);
    org::minima::objects::base::MiniNumber sumOutputs();
    org::minima::objects::base::MiniNumber sumOutputs(const org::minima::objects::base::MiniData& zTokenID);

    // Validation
    bool checkValid();

    // Burn amount (inputs - outputs)
    org::minima::objects::base::MiniNumber getBurn();

    // State variables
    void addStateVariable(std::unique_ptr<org::minima::objects::StateVariable> zValue);
    void removeStateVariable(int zPort);
    org::minima::objects::StateVariable* getStateValue(int zPort);
    const org::minima::objects::StateVariable* getStateValue(int zPort) const;
    bool stateExists(int zStateNum) const;
    void clearState();

    std::vector<std::unique_ptr<org::minima::objects::StateVariable>>& getCompleteState();
    const std::vector<std::unique_ptr<org::minima::objects::StateVariable>>& getCompleteState() const;

    // Link hash
    org::minima::objects::base::MiniData getLinkHash() const;
    void setLinkHash(const org::minima::objects::base::MiniData& zLinkHash);

    // Transaction ID
    void calculateTransactionID();
    org::minima::objects::base::MiniData getTransactionID() const;

    // Calculate coin ID for an output
    org::minima::objects::base::MiniData calculateCoinID(const org::minima::objects::base::MiniData& zBaseCoinID, int zOutput);

    // JSON
    std::string toString();
    org::minima::utils::json::JSONObject toJSON() const;

    // State size calculation (serialized)
    long long calculateStateSize() const;

    // Streamable interface
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Public flags (as in Java)
    bool mHaveCheckedMonotonic = false;
    bool mIsMonotonic = false;
    bool mIsValid = false;

private:
    std::unique_ptr<org::minima::objects::base::MiniData> mLinkHash;        // default ZERO_TXPOWID
    std::vector<std::shared_ptr<org::minima::objects::Coin>> mInputs; // <--- CHANGED
    std::vector<std::unique_ptr<org::minima::objects::Coin>> mOutputs;
    std::vector<std::unique_ptr<org::minima::objects::StateVariable>> mState;
    std::unique_ptr<org::minima::objects::base::MiniData> mTransactionID;   // default ZERO_TXPOWID

    // Helpers for sorting by port
    static bool stateVarPortLess(const std::unique_ptr<org::minima::objects::StateVariable>& a,
                                 const std::unique_ptr<org::minima::objects::StateVariable>& b);
};

} // namespace objects
} // namespace minima
} // namespace org
