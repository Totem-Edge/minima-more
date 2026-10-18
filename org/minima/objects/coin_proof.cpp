#include "org/minima/objects/coin_proof.hpp"

#include <sstream>
#include <vector>
#include <stdexcept>
#include <any> // Required for std::any

#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
// FIX 1: Add the full include for MMRProof
#include "org/minima/objects/mmr/m_m_r_proof.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace objects {

CoinProof::CoinProof()
    : mCoin(), mProof() {}

CoinProof::CoinProof(std::shared_ptr<org::minima::objects::Coin> zCoin,
                     std::shared_ptr<org::minima::objects::mmr::MMRProof> zProof)
    : mCoin(std::move(zCoin)), mProof(std::move(zProof)) {}

const org::minima::objects::Coin& CoinProof::getCoin() const {
    if (!mCoin) {
        throw std::runtime_error("CoinProof::getCoin called but mCoin is null");
    }
    return *mCoin;
}

org::minima::objects::Coin& CoinProof::getCoin() {
    if (!mCoin) {
        throw std::runtime_error("CoinProof::getCoin called but mCoin is null");
    }
    return *mCoin;
}

const org::minima::objects::mmr::MMRProof& CoinProof::getMMRProof() const {
    if (!mProof) {
        throw std::runtime_error("CoinProof::getMMRProof called but mProof is null");
    }
    return *mProof;
}

org::minima::objects::mmr::MMRProof& CoinProof::getMMRProof() {
    if (!mProof) {
        throw std::runtime_error("CoinProof::getMMRProof called but mProof is null");
    }
    return *mProof;
}

CoinProof::CoinProof(const CoinProof& zOther)
    : mCoin(zOther.mCoin),     // This copies the shared_ptr
      mProof(zOther.mProof)  // This copies the shared_ptr
{
    // Nothing else to do
}

std::unique_ptr<org::minima::objects::mmr::MMRData> CoinProof::getMMRData() const {
    // Functional equivalent to Java:
    // return MMRData.CreateMMRDataLeafNode(getCoin(), getCoin().getAmount());
    return org::minima::objects::mmr::MMRData::CreateMMRDataLeafNode(
        const_cast<org::minima::objects::Coin&>(getCoin()), // Needs const_cast if CreateMMRDataLeafNode takes non-const Coin&
        getCoin().getAmount()
    );
}

org::minima::utils::json::JSONObject CoinProof::toJSON() const {
    org::minima::utils::json::JSONObject ret;
    // JSONObject::put expects std::any; wrap nested JSON objects
    ret.put("coin", mCoin ? std::any(mCoin->toJSON()) : std::any());
    // Now MMRProof definition is included, this will work
    ret.put("proof", mProof ? std::any(mProof->toJSON()) : std::any());
    return ret;
}

std::unique_ptr<CoinProof> CoinProof::convertMiniDataVersion(const org::minima::objects::base::MiniData& zTxpData) {
    std::unique_ptr<CoinProof> txnrow;
    try {
        const std::vector<std::uint8_t>& bytes = zTxpData.getBytes();
        std::string buf(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        std::istringstream iss(buf, std::ios::binary);

        txnrow = CoinProof::ReadFromStream(iss);
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
        txnrow.reset(); // Ensure nullptr is returned on error
    }
    return txnrow;
}

void CoinProof::writeDataStream(std::ostream& out) {
    if (!mCoin || !mProof) {
        throw std::runtime_error("CoinProof::writeDataStream - nullptr member(s)");
    }
    // Coin first via std::ostream
    mCoin->writeDataStream(out);
    // Then the proof via std::ostream (now definition is included)
    mProof->writeDataStream(out);
}

void CoinProof::readDataStream(std::istream& in) {
    // Read Coin value via its static helper
    auto coin_val = org::minima::objects::Coin::ReadFromStream(in);
mCoin = std::move(coin_val); // This moves the unique_ptr

    // Read MMRProof value via its static helper (definition now included)
    org::minima::objects::mmr::MMRProof proof_val = org::minima::objects::mmr::MMRProof::ReadFromStream(in);
    mProof = std::make_shared<org::minima::objects::mmr::MMRProof>(std::move(proof_val));
}

// Note: ReadFromStream returns unique_ptr as per your header
std::unique_ptr<CoinProof> CoinProof::ReadFromStream(std::istream& in) {
    // Use private constructor and readDataStream
    auto cp = std::unique_ptr<CoinProof>(new CoinProof());
    try {
        cp->readDataStream(in);
    } catch (const std::exception& e) {
         org::minima::utils::MinimaLogger::log("Error reading CoinProof from stream: " + std::string(e.what()));
         return nullptr; // Return nullptr on error
    }
    return cp;
}

} // namespace objects
} // namespace minima
} // namespace org
