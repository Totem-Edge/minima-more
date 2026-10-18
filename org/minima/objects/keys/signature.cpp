#include "org/minima/objects/keys/signature.hpp"

#include <sstream>
#include <utility>
#include <stdexcept>
#include <cstdint>

#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/objects/keys/signature_proof.hpp"


namespace org {
namespace minima {
namespace objects {
namespace keys {

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::utils::MinimaLogger;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;

Signature::Signature() = default;
Signature::~Signature() = default;
Signature::Signature(Signature&&) noexcept = default;
Signature& Signature::operator=(Signature&&) noexcept = default;

void Signature::addSignatureProof(std::unique_ptr<SignatureProof> zSignature) {
    mSignatures.emplace_back(std::move(zSignature));
}

const std::vector<std::unique_ptr<SignatureProof>>& Signature::getAllSignatureProofs() const {
    return mSignatures;
}

MiniData Signature::getRootPublicKey() const {
    if (mSignatures.empty()) {
        throw std::out_of_range("Signature::getRootPublicKey: no signature proofs available");
    }
    return mSignatures.at(0)->getRootPublicKey();
}

JSONObject Signature::toJSON() const {
    JSONObject json;

    JSONArray sigs;
    for (const auto& sig : mSignatures) {
        sigs.add(sig->toJSON());
    }
    json.put("signatures", sigs);

    return json;
}

Signature::Signature(const Signature& zOther)
{
    // Manually deep-copy the vector of unique_ptrs
    mSignatures.reserve(zOther.mSignatures.size());
    for (const auto& sig : zOther.mSignatures) {
        // This assumes SignatureProof is copyable (it won't be, that's our next fix)
        mSignatures.push_back(sig ? std::make_unique<SignatureProof>(*sig) : nullptr);
    }
}

void Signature::writeDataStream(std::ostream& out) {
    // Write number of signatures
    MiniNumber::WriteToStream(out, static_cast<int>(mSignatures.size()));
    // Write each SignatureProof
    for (const auto& sig : mSignatures) {
        sig->writeDataStream(out);
    }
}

void Signature::readDataStream(std::istream& in) {
    mSignatures.clear();

    int len = MiniNumber::ReadFromStream(in).getAsInt();
    if (len < 0) {
        throw std::runtime_error("Signature::readDataStream: negative length");
    }
    mSignatures.reserve(static_cast<std::size_t>(len));
    for (int i = 0; i < len; ++i) {
        std::unique_ptr<SignatureProof> sig = SignatureProof::ReadFromStream(in);
        mSignatures.emplace_back(std::move(sig));
    }
}

std::unique_ptr<Signature> Signature::ReadFromStream(std::istream& in) {
    auto sigtree = std::make_unique<Signature>();
    sigtree->readDataStream(in);
    return sigtree;
}

std::unique_ptr<Signature> Signature::convertMiniDataVersion(const MiniData& zTxpData) {
    std::unique_ptr<Signature> signature;

    try {
        const std::vector<std::uint8_t>& bytes = zTxpData.getBytes();
        std::string buffer(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        std::istringstream iss(buffer, std::ios::binary);

        signature = Signature::ReadFromStream(iss);
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
    }

    return signature;
}

} // namespace keys
} // namespace objects
} // namespace minima
} // namespace org