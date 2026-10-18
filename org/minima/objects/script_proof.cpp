#include "org/minima/objects/script_proof.hpp"

#include <stdexcept>

#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_entry.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

// Address full include (assumed path by package structure)
#include "org/minima/objects/address.hpp"

#ifdef _WIN32
// No Windows-specific behavior needed currently.
#endif

namespace org {
namespace minima {
namespace objects {

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniString;
using org::minima::objects::mmr::MMR;
using org::minima::objects::mmr::MMRData;
using org::minima::objects::mmr::MMREntry;
using org::minima::objects::mmr::MMRProof;
using org::minima::utils::json::JSONObject;

// Private default ctor
ScriptProof::ScriptProof() : mScript(), mProof(nullptr), mAddress(nullptr) {}

// Public ctors
ScriptProof::ScriptProof(const std::string& zScript)
    : mScript(zScript)
    , mProof(nullptr)
    , mAddress(nullptr) 
{
    // Create an MMR and add the script as a leaf with value ZERO
    MMR mmr;
    std::unique_ptr<MMRData> scriptdata = MMRData::CreateMMRDataLeafNode(mScript, MiniNumber::ZERO());

    // Add to the MMR
    MMREntry entry = mmr.addEntry(*scriptdata);

    // Get the MMRProof
    MMRProof prf = mmr.getProof(entry.getEntryNumber());
    mProof = std::make_unique<MMRProof>(std::move(prf));

    // Calculate the root address
    calculateAddress();
}

ScriptProof::ScriptProof(const std::string& zScript, const MMRProof& zProof)
    : mScript(zScript)
    , mProof(std::make_unique<MMRProof>(zProof))
    , mAddress(nullptr)
{
    calculateAddress();
}

// Destructor and move specials (defined here to ensure complete types availability - Pitfall 7)
ScriptProof::~ScriptProof() = default;
ScriptProof::ScriptProof(ScriptProof&&) noexcept = default;
ScriptProof& ScriptProof::operator=(ScriptProof&&) noexcept = default;

ScriptProof::ScriptProof(const ScriptProof& zOther)
    : mScript(zOther.mScript) // MiniString is copyable
{
    // Manually deep-copy unique_ptr members
    if (zOther.mProof) {
        // This assumes MMRProof is copyable (it won't be, that's our next fix)
        mProof = std::make_unique<org::minima::objects::mmr::MMRProof>(*zOther.mProof);
    } else {
        mProof = nullptr;
    }

    if (zOther.mAddress) {
        // This assumes Address is copyable
        mAddress = std::make_unique<Address>(*zOther.mAddress);
    } else {
        mAddress = nullptr;
    }
}

// Accessors
const MiniString& ScriptProof::getScript() const {
    return mScript;
}

const MMRProof& ScriptProof::getProof() const {
    if (!mProof) {
        throw std::runtime_error("ScriptProof::getProof called but proof is not set");
    }
    return *mProof;
}

const Address& ScriptProof::getAddress() const {
    if (!mAddress) {
        throw std::runtime_error("ScriptProof::getAddress called but address is not set");
    }
    return *mAddress;
}

MiniData ScriptProof::getAddressData() const {
    if (!mAddress) {
        throw std::runtime_error("ScriptProof::getAddressData called but address is not set");
    }
    return mAddress->getAddressData();
}

// JSON
JSONObject ScriptProof::toJSON() const {
    JSONObject json;
    json.put("script", mScript.toString());
    if (!mAddress) {
        throw std::runtime_error("ScriptProof::toJSON address not set");
    }
    json.put("address", mAddress->getAddressData().to0xString());
    if (!mProof) {
        throw std::runtime_error("ScriptProof::toJSON proof not set");
    }
    json.put("proof", mProof->toJSON());
    return json;
}

// Streamable
void ScriptProof::writeDataStream(std::ostream& out) {
    mScript.writeDataStream(out);
    if (!mProof) {
        throw std::runtime_error("ScriptProof::writeDataStream proof not set");
    }
    mProof->writeDataStream(out);
}

void ScriptProof::readDataStream(std::istream& in) {
    mScript = MiniString::ReadFromStream(in);
    MMRProof rp = MMRProof::ReadFromStream(in);
    mProof = std::make_unique<MMRProof>(std::move(rp));
    calculateAddress();
}

// Static ReadFromStream
ScriptProof ScriptProof::ReadFromStream(std::istream& in) {
    ScriptProof pscr;
    pscr.readDataStream(in);
    return pscr;
}

// Private helper
void ScriptProof::calculateAddress() {
    if (!mProof) {
        throw std::runtime_error("ScriptProof::calculateAddress called without proof");
    }

    // Create a leaf node for the current script
    std::unique_ptr<MMRData> scriptdata = MMRData::CreateMMRDataLeafNode(mScript, MiniNumber::ZERO());

    // Calculate the final root
    std::unique_ptr<MMRData> root = mProof->calculateProof(*scriptdata);

    // The address is the final hash
    mAddress = std::make_unique<Address>(root->getData());
}

} // namespace objects
} // namespace minima
} // namespace org