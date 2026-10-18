#pragma once

#include <memory>
#include <vector>
#include <string>

#include "org/minima/utils/streamable.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_byte.hpp"
#include "org/minima/objects/mmr/m_m_r_entry_number.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

namespace org {
namespace minima {
namespace objects {

// Forward declarations to avoid heavy includes in header (PITFALL 4)
class StateVariable;
class Token;
class Address;

class Coin : public org::minima::utils::Streamable {
public:
    // Java: public static final MiniData COINID_OUTPUT/ELTOO
    static const org::minima::objects::base::MiniData COINID_OUTPUT;
    static const org::minima::objects::base::MiniData COINID_ELTOO;

    // Constructors (Java overloads)
    Coin(const org::minima::objects::base::MiniData& zAddress,
         const org::minima::objects::base::MiniNumber& zAmount,
         const org::minima::objects::base::MiniData& zTokenID);

    Coin(const org::minima::objects::base::MiniData& zAddress,
         const org::minima::objects::base::MiniNumber& zAmount,
         const org::minima::objects::base::MiniData& zTokenID,
         bool zStoreState);

    Coin(const org::minima::objects::base::MiniData& zCoinID,
         const org::minima::objects::base::MiniData& zAddress,
         const org::minima::objects::base::MiniNumber& zAmount,
         const org::minima::objects::base::MiniData& zTokenID);

    Coin(const org::minima::objects::base::MiniData& zCoinID,
         const org::minima::objects::base::MiniData& zAddress,
         const org::minima::objects::base::MiniNumber& zAmount,
         const org::minima::objects::base::MiniData& zTokenID,
         bool zStoreState);

    // Destructor and special members for unique_ptr to incomplete types (PITFALL 1)
    virtual ~Coin();
    Coin(Coin&&) noexcept;
    Coin& operator=(Coin&&) noexcept;

    Coin(const Coin& zOther); // Declare the copy constructor
    Coin& operator=(const Coin&) = delete;

    // Return the same Coin but with a new CoinID (deep copy then modify)
    std::unique_ptr<Coin> getSameCoinWithCoinID(const org::minima::objects::base::MiniData& zCoinID) const;

    // Mutators
    void resetCoinID(const org::minima::objects::base::MiniData& zCoinID);
    void resetTokenID(const org::minima::objects::base::MiniData& zTokenID);

    // Token accessors
    const Token* getToken() const;
    Token* getToken();
    void setToken(std::unique_ptr<Token> zToken);

    // MMR entry number
    void setMMREntryNumber(const org::minima::objects::mmr::MMREntryNumber& zEntryNumber);
    org::minima::objects::mmr::MMREntryNumber getMMREntryNumber() const;

    // Spent flag
    void setSpent(bool zSpent);
    bool getSpent() const;

    // Block created
    void setBlockCreated(const org::minima::objects::base::MiniNumber& zBlock);
    org::minima::objects::base::MiniNumber getBlockCreated() const;

    // Store state?
    bool storeState() const;

    // Basic getters
    org::minima::objects::base::MiniData getCoinID() const;
    org::minima::objects::base::MiniData getAddress() const;
    org::minima::objects::base::MiniNumber getAmount() const;
    org::minima::objects::base::MiniData getTokenID() const;

    // Token amount scaling
    org::minima::objects::base::MiniNumber getTokenAmount() const;

    // State
    std::vector<std::unique_ptr<StateVariable>>& getState();
    const std::vector<std::unique_ptr<StateVariable>>& getState() const;
    void setState(std::vector<std::unique_ptr<StateVariable>> zCompleteState);

    bool checkForStateVariable(const std::string& zCheckState) const;
    bool checkForStateVariable(const std::string& zCheckState, bool zWildcard) const;

    // JSON
    std::string toString() const;
    org::minima::utils::json::JSONObject toJSON() const;
    org::minima::utils::json::JSONObject toJSON(bool zSimpleState) const;

    org::minima::utils::json::JSONObject getStateAsJSON() const;
    static org::minima::utils::json::JSONObject convertStateListToJSON(
        const std::vector<std::unique_ptr<StateVariable>>& zStateList);

    // Convert from MiniData blob to Coin
    static std::unique_ptr<Coin> convertMiniDataVersion(const org::minima::objects::base::MiniData& zTxpData);

    // Deep copy (via serialization)
    std::unique_ptr<Coin> deepCopy() const;

    // Streamable
    void writeDataStream(std::ostream& zOut) override;
    void readDataStream(std::istream& zIn) override;

    static std::unique_ptr<Coin> ReadFromStream(std::istream& zIn);

private:
    // Private default constructor for internal use (ReadFromStream, deepCopy)
    Coin();

    // Members
    org::minima::objects::base::MiniData   mCoinID;
    org::minima::objects::base::MiniData   mAddress;
    org::minima::objects::base::MiniNumber mAmount;
    org::minima::objects::base::MiniData   mTokenID;

    bool mStoreState = true;

    std::vector<std::unique_ptr<StateVariable>> mState;

    org::minima::objects::mmr::MMREntryNumber mMMREntryNumber;
    org::minima::objects::base::MiniByte      mSpent;
    org::minima::objects::base::MiniNumber    mBlockCreated;

    std::unique_ptr<Token> mToken;
};

} // namespace objects
} // namespace minima
} // namespace org