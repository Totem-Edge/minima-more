#pragma once

#include <memory>
#include <string>
#include <ostream>
#include <istream>

#include "org/minima/utils/streamable.hpp"

// Namespaced forward declarations
namespace org { namespace minima { namespace objects { class Coin; } } }
namespace org { namespace minima { namespace objects { namespace mmr { class MMRProof; class MMRData; } } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; } } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace objects {

class CoinProof : public org::minima::utils::Streamable {
public:
    CoinProof();
    CoinProof(std::shared_ptr<org::minima::objects::Coin> zCoin,
              std::shared_ptr<org::minima::objects::mmr::MMRProof> zProof);

    CoinProof(const CoinProof& zOther);
    CoinProof& operator=(const CoinProof&) = delete;
    CoinProof(CoinProof&&) noexcept = default;
    CoinProof& operator=(CoinProof&&) noexcept = default;

    // Accessors
    const org::minima::objects::Coin& getCoin() const;
    org::minima::objects::Coin& getCoin();
    const org::minima::objects::mmr::MMRProof& getMMRProof() const;
    org::minima::objects::mmr::MMRProof& getMMRProof();

    // FIX: Add missing semicolons after inline function definitions
    std::shared_ptr<const org::minima::objects::Coin> getCoinPtr() const { return mCoin; }; // <-- Semicolon added
    std::shared_ptr<org::minima::objects::Coin> getCoinPtr() { return mCoin; }; // <-- Semicolon added

    std::shared_ptr<const org::minima::objects::mmr::MMRProof> getProofPtr() const { return mProof; }; // <-- Semicolon added
    std::shared_ptr<org::minima::objects::mmr::MMRProof> getProofPtr() { return mProof; }; // <-- Semicolon added

    std::unique_ptr<org::minima::objects::mmr::MMRData> getMMRData() const;
    org::minima::utils::json::JSONObject toJSON() const;
    static std::unique_ptr<CoinProof> convertMiniDataVersion(const org::minima::objects::base::MiniData& zTxpData);
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;
    static std::unique_ptr<CoinProof> ReadFromStream(std::istream& in);

private:
    std::shared_ptr<org::minima::objects::Coin> mCoin;
    std::shared_ptr<org::minima::objects::mmr::MMRProof> mProof;
}; // <-- This is line 78, the error was likely referring to the missing ';' before this

} // namespace objects
} // namespace minima
} // namespace org
