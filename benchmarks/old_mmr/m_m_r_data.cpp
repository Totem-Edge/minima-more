#include "org/minima/objects/mmr/m_m_r_data.hpp"

#include <utility>
#include <stdexcept>
#include <memory> // Ensure std::make_unique is available

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/utils/crypto.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#ifdef _WIN32
// No OS-specific behavior needed currently; placeholder for future differences
#endif

namespace org {
namespace minima {
namespace objects {
namespace mmr {

// Destructor and move operations definitions (PIMPL fix)
MMRData::~MMRData() = default;
MMRData::MMRData(MMRData&&) noexcept = default;
MMRData& MMRData::operator=(MMRData&&) noexcept = default;

MMRData::MMRData()
    : mData(nullptr)
    , mUnspendable(false) // Initialize mUnspendable
    , mValue(nullptr)     // ..before mValue
    {}

MMRData::MMRData(const org::minima::objects::base::MiniData& zHash,
                 const org::minima::objects::base::MiniNumber& zValue)
    : mData(std::make_unique<org::minima::objects::base::MiniData>(zHash))
    , mUnspendable(false) // Initialize mUnspendable
    , mValue(std::make_unique<org::minima::objects::base::MiniNumber>(zValue)) // ..before mValue
    {}

// Clone implementation for deep copy, used by MMREntry
std::unique_ptr<MMRData> MMRData::Clone() const {
    // Return a new unique_ptr containing a deep copy of the current object's data
    return std::make_unique<MMRData>(*mData, *mValue);
}


// FIX: Change return type from MMRData to std::unique_ptr<MMRData>
std::unique_ptr<MMRData> MMRData::CreateMMRDataLeafNode(org::minima::utils::Streamable& zData,
                                       const org::minima::objects::base::MiniNumber& zSumValue) {
    using org::minima::objects::base::MiniData;
    using org::minima::objects::base::MiniNumber;
    using org::minima::utils::Crypto;

    // Prepare non-const Streamable pointers by making local copies where necessary
    MiniNumber zero = MiniNumber::ZERO();
    MiniNumber sumcopy = zSumValue;

    MiniData hash = Crypto::getInstance().hashAllObjects({ &zero, &zData, &sumcopy });

    // FIX: Return a unique_ptr
    return std::make_unique<MMRData>(hash, zSumValue);
}

// FIX: Change return type from MMRData to std::unique_ptr<MMRData>
std::unique_ptr<MMRData> MMRData::CreateMMRDataParentNode(const MMRData& zLeft,
                                         const MMRData& zRight) {
    using org::minima::objects::base::MiniData;
    using org::minima::objects::base::MiniNumber;
    using org::minima::utils::Crypto;

    // Combine the values
    MiniNumber sumvalue = zLeft.getValue().add(zRight.getValue());

    // Make local, non-const copies for hashing
    MiniNumber one = MiniNumber::ONE();
    MiniData leftdata = zLeft.getData();
    MiniData rightdata = zRight.getData();
    MiniNumber sumcopy = sumvalue;

    MiniData combinedhash = Crypto::getInstance().hashAllObjects({ &one, &leftdata, &rightdata, &sumcopy });

    // FIX: Return a unique_ptr
    return std::make_unique<MMRData>(combinedhash, sumvalue);
}

// Accessors (only const versions are declared in the header)
const org::minima::objects::base::MiniData& MMRData::getData() const {
    // Check for null before dereferencing, though usually done in constructor
    if (!mData) {
        throw std::runtime_error("MMRData::getData called with null mData");
    }
    return *mData;
}

// REMOVED: Non-const MiniData& MMRData::getData() (Not declared in header)
// org::minima::objects::base::MiniData& MMRData::getData() {
//     return *mData;
// }

const org::minima::objects::base::MiniNumber& MMRData::getValue() const {
    // Check for null before dereferencing, though usually done in constructor
    if (!mValue) {
        throw std::runtime_error("MMRData::getValue called with null mValue");
    }
    return *mValue;
}

// REMOVED: Non-const MiniNumber& MMRData::getValue() (Not declared in header)
// org::minima::objects::base::MiniNumber& MMRData::getValue() {
//     return *mValue;
// }

void MMRData::setUnspendable(bool zUnspendable) {
    mUnspendable = zUnspendable;
}

bool MMRData::isUnspendable() const {
    return mUnspendable;
}

bool MMRData::isEqual(const MMRData& zData) const {
    if (!mData || !mValue) {
        throw std::runtime_error("MMRData::isEqual called with uninitialized members");
    }
    return mData->isEqual(zData.getData()) && mValue->isEqual(zData.getValue());
}

org::minima::utils::json::JSONObject MMRData::toJSON() const {
    org::minima::utils::json::JSONObject obj;
    if (!mData || !mValue) {
         throw std::runtime_error("MMRData::toJSON called with uninitialized members");
    }
    obj.put("data", mData->to0xString());
    obj.put("value", mValue->toString());
    return obj;
}

std::string MMRData::toString() const {
    return toJSON().toString();
}

void MMRData::writeDataStream(std::ostream& out) {
    if (!mData || !mValue) {
         throw std::runtime_error("MMRData::writeDataStream called with uninitialized members");
    }
    // Assuming MiniData::writeHashToStream and MiniNumber::writeDataStream exist and work with std::ostream
    mData->writeHashToStream(out);
    mValue->writeDataStream(out);
}

void MMRData::readDataStream(std::istream& in) {
    using org::minima::objects::base::MiniData;
    using org::minima::objects::base::MiniNumber;

    // Assuming MiniData::ReadHashFromStream and MiniNumber::ReadFromStream return
    // MiniData and MiniNumber by value, which is then copied into the unique_ptrs.
    MiniData md = MiniData::ReadHashFromStream(in);
    MiniNumber mv = MiniNumber::ReadFromStream(in);

    mData = std::make_unique<MiniData>(md);
    mValue = std::make_unique<MiniNumber>(mv);
}

// FIX: Change return type from MMRData to std::unique_ptr<MMRData>
std::unique_ptr<MMRData> MMRData::ReadFromStream(std::istream& in) {
    // Create a local MMRData object on the stack
    MMRData data;
    // Read the data into the local object
    data.readDataStream(in);
    // Move the fully constructed object into a unique_ptr and return it
    return std::make_unique<MMRData>(std::move(data));
}

} // namespace mmr
} // namespace objects
} // namespace minima
} // namespace org