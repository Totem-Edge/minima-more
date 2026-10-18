#include "org/minima/objects/keys/signature_proof.hpp"

#include <utility>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/minima_logger.hpp"

namespace org {
namespace minima {
namespace objects {
namespace keys {

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::mmr::MMRData;
using org::minima::objects::mmr::MMRProof;
using org::minima::utils::json::JSONObject;

// Special members definitions (Pitfall 1)
// SignatureProof::~SignatureProof() = default;
// SignatureProof::SignatureProof(SignatureProof&&) noexcept = default;
// SignatureProof& SignatureProof::operator=(SignatureProof&&) noexcept = default;

// Special members definitions (Pitfall 1)
SignatureProof::~SignatureProof() = default;

// --- DEFINED MOVE CONSTRUCTOR ---
SignatureProof::SignatureProof(SignatureProof&& zOther) noexcept
    : mPublicKey(std::move(zOther.mPublicKey))
    , mSignature(std::move(zOther.mSignature))
    , mProof(std::move(zOther.mProof))
{
    // Log the move
    // org::minima::utils::MinimaLogger::log(
    //     "DEBUG_SP: Move Constructor. this=" + 
    //     std::to_string(reinterpret_cast<uintptr_t>(this)) +
    //     " from=" + std::to_string(reinterpret_cast<uintptr_t>(&zOther))
    // );
    // zOther's pointers are now nullptr
}

// --- DEFINED MOVE ASSIGNMENT ---
SignatureProof& SignatureProof::operator=(SignatureProof&& zOther) noexcept {
    // org::minima::utils::MinimaLogger::log(
    //     "DEBUG_SP: Move Assignment. this=" + 
    //     std::to_string(reinterpret_cast<uintptr_t>(this)) +
    //     " from=" + std::to_string(reinterpret_cast<uintptr_t>(&zOther))
    // );
    if (this != &zOther) {
        mPublicKey = std::move(zOther.mPublicKey);
        mSignature = std::move(zOther.mSignature);
        mProof     = std::move(zOther.mProof);
    }
    return *this;
}

// Private default constructor
// SignatureProof::SignatureProof() = default;
SignatureProof::SignatureProof()
    : mPublicKey(std::make_unique<MiniData>())
    , mSignature(std::make_unique<MiniData>())
    , mProof(std::make_unique<MMRProof>()) {
    
    
    // org::minima::utils::MinimaLogger::log(
    //     "DEBUG_SP: Default Constructor. this=" + 
    //     std::to_string(reinterpret_cast<uintptr_t>(this))
    // );
}

// Public constructor
SignatureProof::SignatureProof(const MiniData& zPublicKey,
                               const MiniData& zSignature,
                               const MMRProof& zProof)
    : mPublicKey(std::make_unique<MiniData>(zPublicKey))
    , mSignature(std::make_unique<MiniData>(zSignature))
    , mProof(std::make_unique<MMRProof>(zProof)) {

    
    // org::minima::utils::MinimaLogger::log(
    //     "DEBUG_SP: Public Constructor. this=" + 
    //     std::to_string(reinterpret_cast<uintptr_t>(this))
    // );
}

SignatureProof::SignatureProof(const SignatureProof& zOther)
    : mPublicKey(zOther.mPublicKey ? 
                 std::make_unique<org::minima::objects::base::MiniData>(*zOther.mPublicKey) 
                 : std::make_unique<org::minima::objects::base::MiniData>())
    , mSignature(zOther.mSignature ? 
                   std::make_unique<org::minima::objects::base::MiniData>(*zOther.mSignature) 
                   : std::make_unique<org::minima::objects::base::MiniData>())
    , mProof(zOther.mProof ? 
             std::make_unique<org::minima::objects::mmr::MMRProof>(*zOther.mProof) 
             : std::make_unique<org::minima::objects::mmr::MMRProof>())
{
    
    // org::minima::utils::MinimaLogger::log(
    //     "DEBUG_SP: Copy Constructor. this=" + 
    //     std::to_string(reinterpret_cast<uintptr_t>(this)) +
    //     " from=" + std::to_string(reinterpret_cast<uintptr_t>(&zOther))
    // );
}

// Accessors
const MiniData& SignatureProof::getPublicKey() const {
    return *mPublicKey;
}

const MiniData& SignatureProof::getSignature() const {
    return *mSignature;
}

const MMRProof& SignatureProof::getProof() const {
    return *mProof;
}

// Compute the root public key
MiniData SignatureProof::getRootPublicKey() const {
    // Create the MMR data entry for the public key with value ZERO
    std::unique_ptr<MMRData> pubentry = MMRData::CreateMMRDataLeafNode(
        const_cast<MiniData&>(*mPublicKey), // CreateMMRDataLeafNode expects Streamable&
        MiniNumber::ZERO()
    );

    // Calculate the proof result
    std::unique_ptr<MMRData> result = mProof->calculateProof(*pubentry);

    // Return a copy of the resulting data (MiniData)
    return result->getData();
}

// JSON
JSONObject SignatureProof::toJSON() const {
    JSONObject json;
    json.put("publickey", mPublicKey->to0xString());
    json.put("rootkey", getRootPublicKey().to0xString());
    json.put("proof", mProof->toJSON());
    json.put("signature", mSignature->to0xString());
    return json;
}

// Streamable write
void SignatureProof::writeDataStream(std::ostream& out) {
    mPublicKey->writeDataStream(out);
    mSignature->writeDataStream(out);
    mProof->writeDataStream(out);
}

// Streamable read
void SignatureProof::readDataStream(std::istream& in) {
    MiniData pk = MiniData::ReadFromStream(in);
    MiniData sig = MiniData::ReadFromStream(in);
    MMRProof proof = MMRProof::ReadFromStream(in);

    mPublicKey = std::make_unique<MiniData>(std::move(pk));
    mSignature = std::make_unique<MiniData>(std::move(sig));
    mProof     = std::make_unique<MMRProof>(std::move(proof));
}

// Static read helper
std::unique_ptr<SignatureProof> SignatureProof::ReadFromStream(std::istream& in) {
    auto sp = std::unique_ptr<SignatureProof>(new SignatureProof());
    sp->readDataStream(in);
    return sp;
}

} // namespace keys
} // namespace objects
} // namespace minima
} // namespace org