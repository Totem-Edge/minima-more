#include "org/minima/objects/address.hpp"

#include <stdexcept>
#include <vector>
#include <cstdint>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_entry.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"
#include "org/minima/utils/base_converter.hpp"
#include "org/minima/utils/crypto.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace objects {

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniString;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::mmr::MMR;
using org::minima::objects::mmr::MMRData;
using org::minima::objects::mmr::MMREntry;
using org::minima::objects::mmr::MMRProof;
using org::minima::utils::BaseConverter;
using org::minima::utils::Crypto;
using org::minima::utils::json::JSONObject;

// Special members for unique_ptr to forward-declared types
Address::~Address() = default;
Address::Address(Address&&) noexcept = default;
Address& Address::operator=(Address&&) noexcept = default;

Address::Address() = default;

Address::Address(const std::string& zScript) {
    // Store the script
    mScript = std::make_unique<MiniString>(zScript);

    // Create an MMR proof..
    MMR mmr;

    // Create a new piece of data to add
    std::unique_ptr<MMRData> scriptdata = MMRData::CreateMMRDataLeafNode(*mScript, MiniNumber::ZERO());

    // Add to the MMR
    MMREntry entry = mmr.addEntry(*scriptdata);

    // Get the MMRProof
    MMRProof proof = mmr.getProof(entry.getEntryNumber());

    // And calculate the final root..
    std::unique_ptr<MMRData> root = proof.calculateProof(*scriptdata);

    // The address is the final hash
    mAddressData = std::make_unique<MiniData>(root->getData());

    // The Minima address as short as can be..
    mMinimaAddress = makeMinimaAddress(*mAddressData);
}


// Address::Address(const std::string& zScript) {
//     // Store the script
//     mScript = std::make_unique<MiniString>(zScript);

//     // ###############################################################
//     // ## RE-WRITE THE CONSTRUCTOR LOGIC
//     // ##
//     // ## The Java MMR logic is a 
//     // ## performance trap. Replace it with a simple, fast hash
//     // ## to get the address, as the rest of the Java code implies.
//     // ###############################################################

//     // 1. Get the script bytes (this assumes MiniString::getBytes() or similar)
//     //    If MiniString doesn't have .getBytes(), convert zScript to std::vector<uint8_t>
//     const std::vector<std::uint8_t> script_bytes(zScript.begin(), zScript.end());

//     // 2. Hash the script bytes (e.g., using SHA3-256)
//     std::vector<std::uint8_t> hash = Crypto::getInstance().hashData(script_bytes);

//     // 3. The address is the final hash
//     mAddressData = std::make_unique<MiniData>(hash);
    
//     // 4. The Minima address as short as can be..
//     mMinimaAddress = makeMinimaAddress(*mAddressData);

// }

Address::Address(const MiniData& zAddressData) {
    mScript = std::make_unique<MiniString>(std::string(""));
    mAddressData = std::make_unique<MiniData>(zAddressData);
    mMinimaAddress = makeMinimaAddress(*mAddressData);
}

Address::Address(const Address& zOther)
    : mMinimaAddress(zOther.mMinimaAddress)
{
    // Manually deep-copy unique_ptr members
    if (zOther.mScript) {
        mScript = std::make_unique<org::minima::objects::base::MiniString>(*zOther.mScript);
    }
    if (zOther.mAddressData) {
        mAddressData = std::make_unique<org::minima::objects::base::MiniData>(*zOther.mAddressData);
    }
}

JSONObject Address::toJSON() const {
    JSONObject addr;
    addr.put("script", mScript ? mScript->toString() : std::string(""));
    addr.put("hexaddress", mAddressData ? mAddressData->toString() : std::string(""));
    addr.put("miniaddress", mMinimaAddress);
    return addr;
}

std::string Address::toString() const {
    return mAddressData ? mAddressData->toString() : std::string("");
}

std::string Address::getScript() const {
    return mScript ? mScript->toString() : std::string("");
}

MiniData Address::getAddressData() const {
    return mAddressData ? *mAddressData : MiniData();
}

std::string Address::getMinimaAddress() const {
    return mMinimaAddress;
}

bool Address::isEqual(const Address& zAddress) const {
    if (!mAddressData) return false;
    return mAddressData->isEqual(zAddress.getAddressData());
}

void Address::writeDataStream(std::ostream& out) {
    if (!mAddressData || !mScript) {
        throw std::runtime_error("Address::writeDataStream - uninitialized data");
    }
    mAddressData->writeHashToStream(out);
    mScript->writeDataStream(out);
}

void Address::readDataStream(std::istream& in) {
    MiniData md = MiniData::ReadHashFromStream(in);
    MiniString ms = MiniString::ReadFromStream(in);

    mAddressData = std::make_unique<MiniData>(md);
    mScript = std::make_unique<MiniString>(ms);
    mMinimaAddress = makeMinimaAddress(*mAddressData);
}

Address Address::ReadFromStream(std::istream& in) {
    Address addr;
    addr.readDataStream(in);
    return addr; // NRVO/move
}

/**
 * Convert an address into a Minima Checksum Base32 address - MAX 32k
 */
std::string Address::makeMinimaAddress(const MiniData& zAddress) {
    // The Original data
    const std::vector<std::uint8_t>& data = zAddress.getBytes();
    const int datalen = static_cast<int>(data.size());

    // First hash it for checksum digits..
    std::vector<std::uint8_t> hash = Crypto::getInstance().hashData(data);
    if (hash.size() < 4) {
        throw std::invalid_argument("Invalid MxAddress - hash too short");
    }
    std::uint8_t checksum[4];
    for (int i = 0; i < 4; ++i) {
        checksum[i] = hash[i];
    }

    // Build the byte array as DataOutputStream would (big-endian short)
    std::vector<std::uint8_t> origdata;
    origdata.reserve(1 + 2 + datalen + 4);

    // MUST write 1 non 0 byte first to ensure no truncation in base 32 conversion
    origdata.push_back(static_cast<std::uint8_t>(1));

    // the length (Java DataOutputStream.writeShort: big-endian 16-bit)
    std::uint16_t len16 = static_cast<std::uint16_t>(datalen & 0xFFFF);
    origdata.push_back(static_cast<std::uint8_t>((len16 >> 8) & 0xFF));
    origdata.push_back(static_cast<std::uint8_t>(len16 & 0xFF));

    // the data itself..
    origdata.insert(origdata.end(), data.begin(), data.end());

    // 4 bytes of the hash
    origdata.insert(origdata.end(), std::begin(checksum), std::end(checksum));

    // Now convert the whole thing to Base 32
    return BaseConverter::encode32(origdata);
}

MiniData Address::convertMinimaAddress(const std::string& zMinimAddress) {
    // First convert the whole thing back..
    std::vector<std::uint8_t> decode = BaseConverter::decode32(zMinimAddress);

    // Now "read" the data from the decoded vector
    size_t idx = 0;
    if (decode.size() < 1 + 2 + 4) {
        throw std::invalid_argument(std::string("Invalid MxAddress : ") + zMinimAddress + " insufficient data");
    }

    // Read the first byte
    int one = decode[idx++];
    if (one != 1) {
        throw std::invalid_argument(std::string("Invalid MxAddress - should start with 1 ") + zMinimAddress);
    }

    // First the data length (big-endian short)
    int datalen = (static_cast<int>(decode[idx]) << 8) | static_cast<int>(decode[idx + 1]);
    idx += 2;

    if (datalen < 0 || decode.size() < idx + static_cast<size_t>(datalen) + 4) {
        throw std::invalid_argument(std::string("Invalid MxAddress : ") + zMinimAddress + " invalid lengths");
    }

    // the data itself..
    std::vector<std::uint8_t> data(decode.begin() + idx, decode.begin() + idx + datalen);
    idx += datalen;

    // And the checksum
    std::uint8_t checksum[4];
    for (int i = 0; i < 4; ++i) {
        checksum[i] = decode[idx + i];
    }
    idx += 4;

    // Now check the hash
    std::vector<std::uint8_t> hash = Crypto::getInstance().hashData(data);
    if (hash.size() < 4) {
        throw std::invalid_argument(std::string("Invalid MxAddress : ") + zMinimAddress + " hash too short");
    }

    // Check the first 4 bytes..
    for (int i = 0; i < 4; ++i) {
        if (hash[i] != checksum[i]) {
            throw std::invalid_argument(std::string("Invalid MxAddress - checksum wrong for ") + zMinimAddress);
        }
    }

    return MiniData(data);
}

// Meyers' singleton for TRUE_ADDRESS
const Address& Address::getTrueAddress() {
    static const std::unique_ptr<Address> s_true = std::make_unique<Address>(std::string("RETURN TRUE"));
    return *s_true;
}

// Optional test main (disabled by default)
// Enable with -DADDRESS_BUILD_MAIN if you need a standalone test.
// This avoids introducing an entry point in library builds.
#ifdef ADDRESS_BUILD_MAIN
#include <iostream>
int main(int argc, char* argv[]) {
    using org::minima::objects::Address;
    using org::minima::objects::base::MiniData;
    using org::minima::utils::BaseConverter;

    MiniData password = MiniData::getRandomData(16);
    std::string b32 = BaseConverter::encode32(password.getBytes());

    auto sub = [&](int start, int end) { return b32.substr(static_cast<size_t>(start), static_cast<size_t>(end - start)); };
    std::string mm = sub(2,6) + "-" + sub(7,11) + "-" + sub(12,16) + "-" + sub(17,21) + "-" + sub(22,26);

    std::cout << "Data     : " << password.to0xString() << std::endl;
    std::cout << "Complete : " << b32 << std::endl;
    std::cout << "Password : " << mm << std::endl;

    return 0;
}
#endif

} // namespace objects
} // namespace minima
} // namespace org