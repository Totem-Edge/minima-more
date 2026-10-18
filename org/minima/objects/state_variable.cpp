#include "org/minima/objects/state_variable.hpp"


#include <algorithm>
#include <cctype>
#include <memory>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/minima_logger.hpp"

namespace org {
namespace minima {
namespace objects {

using org::minima::objects::base::MiniByte;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniString;
using org::minima::utils::json::JSONObject;

// Static constants
const MiniByte StateVariable::STATETYPE_HEX    = MiniByte(1);
const MiniByte StateVariable::STATETYPE_NUMBER = MiniByte(2);
const MiniByte StateVariable::STATETYPE_STRING = MiniByte(4);
const MiniByte StateVariable::STATETYPE_BOOL   = MiniByte(8);

// Private default constructor (Java: private StateVariable() {})
StateVariable::StateVariable()
    : mType(0), mPort(0), mData("") {}

// Helpers
std::string StateVariable::trim(const std::string& s) {
    auto begin = s.find_first_not_of(" \t\n\r\f\v");
    if (begin == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\n\r\f\v");
    return s.substr(begin, end - begin + 1);
}

std::string StateVariable::toLower(const std::string& s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return out;
}

bool StateVariable::iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) return false;
    }
    return true;
}

bool StateVariable::startsWithIgnoreCase(const std::string& s, const std::string& prefix) {
    if (prefix.size() > s.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(s[i])) !=
            std::tolower(static_cast<unsigned char>(prefix[i]))) return false;
    }
    return true;
}

// Constructor mirroring Java logic
StateVariable::StateVariable(int zPort, const std::string& zData) : mType(0), mPort(0), mData("") {
    // Check within range
    if (zPort < 0 || zPort > 255) {
        throw std::invalid_argument("State Variable port MUST be 0-255");
    }

    // Check not blank
    if (trim(zData).empty()) {
        throw std::invalid_argument("State Variable cannot be blank");
    }

    // Store as MiniByte
    mPort = MiniByte(zPort);

    // Set the Data
    if (startsWithIgnoreCase(zData, "mx")) {
        // Java: Address.convertMinimaAddress(zData).to0xString()
        // Not available; avoid guessing behavior.
        throw std::invalid_argument("mx address conversion requires Address.convertMinimaAddress; unavailable in this build");

    } else if (startsWithIgnoreCase(zData, "0x")) {
        // Keep provided hex string as-is (Java canonicalizes via MiniData)
        mData = MiniString(zData);
        mType = STATETYPE_HEX;

    } else if (iequals(zData, "true")) {
        mData = MiniString("TRUE");
        mType = STATETYPE_BOOL;

    } else if (iequals(zData, "false")) {
        mData = MiniString("FALSE");
        mType = STATETYPE_BOOL;

    } else if (!zData.empty() && zData.front() == '[' && zData.back() == ']') {
        mData = MiniString(zData);
        mType = STATETYPE_STRING;

    } else {
        // Normalize number via MiniNumber
        MiniNumber number(zData);
        mData = MiniString(number.toString());
        mType = STATETYPE_NUMBER;
    }
}

int StateVariable::getPort() const {
    return mPort.getValue();
}

const MiniByte& StateVariable::getType() const {
    return mType;
}

const MiniString& StateVariable::getData() const {
    return mData;
}

JSONObject StateVariable::toJSON() const {
    JSONObject ret;
    // Java: ret.put("port", mPort); ret.put("type", mType); ret.put("data", mData.toString());
    // Store string forms to fit std::any-friendly JSON skeleton.
    ret.put("port", mPort.toString());
    ret.put("type", mType.toString());
    ret.put("data", mData.toString());
    return ret;
}

std::string StateVariable::toString() const {
    return mData.toString();
}

void StateVariable::writeDataStream(std::ostream& out) {
    // Port and Type
    mPort.writeDataStream(out);
    mType.writeDataStream(out);

    // org::minima::utils::MinimaLogger::log(
    //     "DEBUG_TXN_HASH: StateVariable type=" + mType.toString() + 
    //     " data=" + mData.toString()
    // );

    // Write the data in the correct format
    if (mType.isEqual(STATETYPE_BOOL)) {
        // Use static helper to avoid const-qualification issue
        MiniByte::WriteToStream(out, mData.isEqual("TRUE"));

    } else if (mType.isEqual(STATETYPE_HEX)) {
        // org::minima::utils::MinimaLogger::log("DEBUG_TXN_HASH: --> Writing as HEX (MiniString)");

        // Create a MiniData from the hex string (e.g., "0xFF")
        org::minima::objects::base::MiniData hexdata(mData.toString());
        // Write the raw bytes (e.g., [ 255 ])
        hexdata.writeDataStream(out);

    } else if (mType.isEqual(STATETYPE_NUMBER)) {
        MiniNumber number(mData.toString());
        number.writeDataStream(out);

    } else if (mType.isEqual(STATETYPE_STRING)) {
        mData.writeDataStream(out);

    } else {
        throw std::ios_base::failure("Invalid StateVariable type during write");
    }
}

void StateVariable::readDataStream(std::istream& in) {
    mPort = MiniByte::ReadFromStream(in);
    mType = MiniByte::ReadFromStream(in);

    if (mType.isEqual(STATETYPE_BOOL)) {
        MiniByte booleanValue = MiniByte::ReadFromStream(in);
        if (booleanValue.isTrue()) {
            mData = MiniString("TRUE");
        } else {
            mData = MiniString("FALSE");
        }

    } else if (mType.isEqual(STATETYPE_HEX)) {
        // Mirror write: Read the raw bytes as MiniData
        org::minima::objects::base::MiniData hexdata = org::minima::objects::base::MiniData::ReadFromStream(in);
        // Convert the raw bytes back to a hex string for storage
        mData = MiniString(hexdata.to0xString());

    } else if (mType.isEqual(STATETYPE_NUMBER)) {
        MiniNumber number = MiniNumber::ReadFromStream(in);
        mData = MiniString(number.toString());

    } else if (mType.isEqual(STATETYPE_STRING)) {
        mData = MiniString::ReadFromStream(in);

    } else {
        throw std::ios_base::failure("Invalid StateVariable type during read");
    }
}

std::unique_ptr<StateVariable> StateVariable::ReadFromStream(std::istream& in) {
    // Use unique_ptr(new StateVariable()) to access the private default constructor
    std::unique_ptr<StateVariable> statevar(new StateVariable());
    statevar->readDataStream(in);
    return statevar;
}

} // namespace objects
} // namespace minima
} // namespace org