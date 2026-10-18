#include "org/minima/objects/witness.hpp"

#include <stdexcept>
#include <utility>
#include <any>

#include "org/minima/objects/keys/signature.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/script_proof.hpp"

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/objects/address.hpp"

namespace org {
namespace minima {
namespace objects {

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::keys::Signature;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;

Witness::Witness()
    : mSignatureProofs()
    , mCoinProofs()
    , mScriptProofs() {
}

Witness::~Witness() = default;
Witness::Witness(Witness&&) noexcept = default;
Witness& Witness::operator=(Witness&&) noexcept = default;


Witness::Witness(const Witness& zOther)
{
    // Manually deep-copy vectors of unique_ptrs

    mSignatureProofs.reserve(zOther.mSignatureProofs.size());
    for (const auto& sig : zOther.mSignatureProofs) {
        // Assumes Signature::deepCopy() exists or it's copyable
        mSignatureProofs.push_back(sig ? std::make_unique<Signature>(*sig) : nullptr);
    }

    mCoinProofs.reserve(zOther.mCoinProofs.size());
    for (const auto& cp : zOther.mCoinProofs) {
        // We already made CoinProof copyable
        mCoinProofs.push_back(cp ? std::make_unique<CoinProof>(*cp) : nullptr);
    }

    mScriptProofs.reserve(zOther.mScriptProofs.size());
    for (const auto& sp : zOther.mScriptProofs) {
        // Assumes ScriptProof::deepCopy() exists or it's copyable
        mScriptProofs.push_back(sp ? std::make_unique<ScriptProof>(*sp) : nullptr);
    }
}

// Signature functions
void Witness::clearSignatures() {
    mSignatureProofs.clear();
}

void Witness::addSignature(std::unique_ptr<Signature> zSigProof) {
    if (!zSigProof) {
        throw std::invalid_argument("Cannot add a NULL Signature");
    }

    // Check not already added
    const std::string rootkey = zSigProof->getRootPublicKey().to0xString();
    if (!isSignedBy(rootkey)) {
        mSignatureProofs.emplace_back(std::move(zSigProof));
    }
}

std::vector<std::unique_ptr<Signature>>& Witness::getAllSignatures() {
    return mSignatureProofs;
}

const std::vector<std::unique_ptr<Signature>>& Witness::getAllSignatures() const {
    return mSignatureProofs;
}

std::vector<MiniData> Witness::getAllSignatureKeys() const {
    std::vector<MiniData> pubkeys;
    pubkeys.reserve(mSignatureProofs.size());
    for (const auto& sigproof : mSignatureProofs) {
        // Get the root key
        pubkeys.emplace_back(sigproof->getRootPublicKey());
    }
    return pubkeys;
}

bool Witness::isSignedBy(const std::string& zPublicKey) const {
    std::vector<MiniData> allkeys = getAllSignatureKeys();
    for (const auto& key : allkeys) {
        if (key.to0xString() == zPublicKey) {
            return true;
        }
    }
    return false;
}

// MMR functions
void Witness::clearCoinProofs() {
    mCoinProofs.clear();
}

void Witness::addCoinProof(std::unique_ptr<CoinProof> zProof) {
    mCoinProofs.emplace_back(std::move(zProof));
}

std::vector<std::unique_ptr<CoinProof>>& Witness::getAllCoinProofs() {
    return mCoinProofs;
}

const std::vector<std::unique_ptr<CoinProof>>& Witness::getAllCoinProofs() const {
    return mCoinProofs;
}

// Script proofs
void Witness::clearScriptProofs() {
    mScriptProofs.clear();
}

void Witness::addScript(std::unique_ptr<ScriptProof> zScriptProof) {
    // Only add if not already present for that address
    if (!scriptExists(zScriptProof->getAddress().getAddressData())) {
        mScriptProofs.emplace_back(std::move(zScriptProof));
    }
}

ScriptProof* Witness::getScript(const MiniData& zAddress) {
    for (auto& proofscr : mScriptProofs) {
        if (proofscr->getAddress().getAddressData().isEqual(zAddress)) {
            return proofscr.get();
        }
    }
    return nullptr;
}

const ScriptProof* Witness::getScript(const MiniData& zAddress) const {
    for (const auto& proofscr : mScriptProofs) {
        if (proofscr->getAddress().getAddressData().isEqual(zAddress)) {
            return proofscr.get();
        }
    }
    return nullptr;
}

std::vector<std::unique_ptr<ScriptProof>>& Witness::getAllScripts() {
    return mScriptProofs;
}

const std::vector<std::unique_ptr<ScriptProof>>& Witness::getAllScripts() const {
    return mScriptProofs;
}

bool Witness::scriptExists(const MiniData& zAddress) const {
    return getScript(zAddress) != nullptr;
}

JSONObject Witness::toJSON() const {
    JSONObject obj;

    // Signatures
    {
        JSONArray arr;
        for (const auto& sg : mSignatureProofs) {
            arr.add(std::any(sg->toJSON()));
        }
        obj.put("signatures", std::any(arr));
    }

    // MMRProofs
    {
        JSONArray arr;
        for (const auto& proof : mCoinProofs) {
            arr.add(std::any(proof->toJSON()));
        }
        obj.put("mmrproofs", std::any(arr));
    }

    // Scripts
    {
        JSONArray arr;
        for (const auto& proofscr : mScriptProofs) {
            arr.add(std::any(proofscr->toJSON()));
        }
        obj.put("scripts", std::any(arr));
    }

    // Tokens.. (not present in Java logic)

    return obj;
}

std::string Witness::toString() const {
    return toJSON().toString();
}

// Streamable
void Witness::writeDataStream(std::ostream& out) {
    // Signatures
    MiniNumber::WriteToStream(out, static_cast<int>(mSignatureProofs.size()));
    for (const auto& sp : mSignatureProofs) {
        sp->writeDataStream(out);
    }

    // MMRProofs
    MiniNumber::WriteToStream(out, static_cast<int>(mCoinProofs.size()));
    for (const auto& cproof : mCoinProofs) {
        cproof->writeDataStream(out);
    }

    // Scripts
    MiniNumber::WriteToStream(out, static_cast<int>(mScriptProofs.size()));
    for (const auto& sp : mScriptProofs) {
        sp->writeDataStream(out);
    }
}

void Witness::readDataStream(std::istream& in) {
    // Signatures
    mSignatureProofs.clear();
    {
        MiniNumber mlen = MiniNumber::ReadFromStream(in);
        int len = mlen.getAsInt();
        mSignatureProofs.reserve(len);
        for (int i = 0; i < len; ++i) {
            auto sig = Signature::ReadFromStream(in);
            mSignatureProofs.emplace_back(std::move(sig));
        }
    }

    // MMRProofs
    mCoinProofs.clear();
    {
        MiniNumber mlen = MiniNumber::ReadFromStream(in);
        int len = mlen.getAsInt();
        mCoinProofs.reserve(len);
        for (int i = 0; i < len; ++i) {
            auto cp = CoinProof::ReadFromStream(in);
            mCoinProofs.emplace_back(std::move(cp));
        }
    }

    // Scripts
    mScriptProofs.clear();
    {
        MiniNumber mlen = MiniNumber::ReadFromStream(in);
        int len = mlen.getAsInt();
        mScriptProofs.reserve(len);
        for (int i = 0; i < len; ++i) {
            // Read the object by value
            auto sp_obj = ScriptProof::ReadFromStream(in);
            // Manually create a unique_ptr and move the object into it
            mScriptProofs.emplace_back(std::make_unique<ScriptProof>(std::move(sp_obj)));
        }
    }
}

} // namespace objects
} // namespace minima
} // namespace org