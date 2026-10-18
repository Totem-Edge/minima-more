#include "org/minima/objects/mmr/m_m_r_proof.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"

namespace org {
namespace minima {
namespace objects {
namespace mmr {

/**********************
 * MMRProofChunk
 **********************/

MMRProof::MMRProofChunk::MMRProofChunk()
    : mLeft(), mMMRData(nullptr) {}

MMRProof::MMRProofChunk::MMRProofChunk(bool zIsLeft, const org::minima::objects::mmr::MMRData& zData)
    : mLeft(org::minima::objects::base::MiniByte(zIsLeft)),
      mMMRData(zData.Clone()) {}

MMRProof::MMRProofChunk::~MMRProofChunk() = default;
MMRProof::MMRProofChunk::MMRProofChunk(MMRProofChunk&&) noexcept = default;
MMRProof::MMRProofChunk& MMRProof::MMRProofChunk::operator=(MMRProofChunk&&) noexcept = default;

MMRProof::MMRProofChunk::MMRProofChunk(const MMRProofChunk& other)
    : mLeft(other.mLeft),
      mMMRData(other.mMMRData ? other.mMMRData->Clone() : nullptr) {}

MMRProof::MMRProofChunk& MMRProof::MMRProofChunk::operator=(const MMRProofChunk& other) {
    if (this != &other) {
        mLeft = other.mLeft;
        if (other.mMMRData) {
            mMMRData = other.mMMRData->Clone();
        } else {
            mMMRData.reset();
        }
    }
    return *this;
}

bool MMRProof::MMRProofChunk::isLeft() const {
    return mLeft.isTrue();
}

const org::minima::objects::mmr::MMRData& MMRProof::MMRProofChunk::getMMRData() const {
    if (!mMMRData) {
        throw std::runtime_error("MMRProofChunk::getMMRData called with null data");
    }
    return *mMMRData;
}

org::minima::utils::json::JSONObject MMRProof::MMRProofChunk::toJSON() const {
    org::minima::utils::json::JSONObject json;
    json.put("left", isLeft());
    if (!mMMRData) {
        throw std::runtime_error("MMRProofChunk::toJSON null MMRData");
    }
    json.put("data", mMMRData->toJSON());
    return json;
}

void MMRProof::MMRProofChunk::writeDataStream(std::ostream& out) {
    mLeft.writeDataStream(out);
    if (!mMMRData) {
        throw std::runtime_error("MMRProofChunk::writeDataStream null MMRData");
    }
    mMMRData->writeDataStream(out);
}

void MMRProof::MMRProofChunk::readDataStream(std::istream& in) {
    mLeft = org::minima::objects::base::MiniByte::ReadFromStream(in);
    auto tmp = org::minima::objects::mmr::MMRData::ReadFromStream(in);
    mMMRData = std::move(tmp);
}

/**********************
 * MMRProof
 **********************/

MMRProof::MMRProof()
    : mBlockTime(org::minima::objects::base::MiniNumber::ZERO()), mProofChain() {}

MMRProof::MMRProof(const org::minima::objects::base::MiniNumber& zBlockTime)
    : mBlockTime(zBlockTime), mProofChain() {}

MMRProof::MMRProof(const MMRProof& zOther)
    : mBlockTime(zOther.mBlockTime),
      mProofChain(zOther.mProofChain) // std::vector can copy MMRProofChunk
{
}

const org::minima::objects::base::MiniNumber& MMRProof::getBlockTime() const {
    return mBlockTime;
}

void MMRProof::addProofChunk(const MMRProof::MMRProofChunk& zChunk) {
    mProofChain.push_back(zChunk);
}

void MMRProof::addProofChunk(bool zIsLeft, const org::minima::objects::mmr::MMRData& zData) {
    mProofChain.emplace_back(zIsLeft, zData);
}

MMRProof::MMRProofChunk& MMRProof::getProofChunk(int zProofIndex) {
    if (zProofIndex < 0 || static_cast<std::size_t>(zProofIndex) >= mProofChain.size()) {
        throw std::out_of_range("MMRProof::getProofChunk index out of range");
    }
    return mProofChain[static_cast<std::size_t>(zProofIndex)];
}

const MMRProof::MMRProofChunk& MMRProof::getProofChunk(int zProofIndex) const {
    if (zProofIndex < 0 || static_cast<std::size_t>(zProofIndex) >= mProofChain.size()) {
        throw std::out_of_range("MMRProof::getProofChunk index out of range (const)");
    }
    return mProofChain[static_cast<std::size_t>(zProofIndex)];
}

int MMRProof::getProofLength() const {
    return static_cast<int>(mProofChain.size());
}

std::unique_ptr<org::minima::objects::mmr::MMRData>
MMRProof::calculateProof(const org::minima::objects::mmr::MMRData& zData) const {
    // Start with a deep copy of the provided data
    std::unique_ptr<org::minima::objects::mmr::MMRData> cmmrdata = zData.Clone();

    // Iterate through the proof chain
    for (const MMRProofChunk& proofchunk : mProofChain) {
        if (proofchunk.isLeft()) {
            cmmrdata = org::minima::objects::mmr::MMRData::CreateMMRDataParentNode(
                proofchunk.getMMRData(), *cmmrdata);
        } else {
            cmmrdata = org::minima::objects::mmr::MMRData::CreateMMRDataParentNode(
                *cmmrdata, proofchunk.getMMRData());
        }
    }

    return cmmrdata;
}

org::minima::utils::json::JSONObject MMRProof::toJSON() const {
    org::minima::utils::json::JSONObject obj;

    obj.put("blocktime", mBlockTime.toString());

    org::minima::utils::json::JSONArray arr;
    for (const MMRProofChunk& chunk : mProofChain) {
        arr.add(chunk.toJSON());
    }

    obj.put("proof", arr);
    obj.put("prooflength", static_cast<int>(mProofChain.size()));

    return obj;
}

std::string MMRProof::toString() const {
    return toJSON().toString();
}

void MMRProof::writeDataStream(std::ostream& out) {
    // BlockTime first
    mBlockTime.writeDataStream(out);

    // Length of the proof chain
    int len = static_cast<int>(mProofChain.size());
    org::minima::objects::base::MiniNumber::WriteToStream(out, len);

    for (int i = 0; i < len; ++i) {
        MMRProofChunk& chunk = mProofChain[static_cast<std::size_t>(i)];
        chunk.writeDataStream(out);
    }
}

void MMRProof::readDataStream(std::istream& in) {
    // BlockTime first
    mBlockTime = org::minima::objects::base::MiniNumber::ReadFromStream(in);

    mProofChain.clear();

    org::minima::objects::base::MiniNumber plen = org::minima::objects::base::MiniNumber::ReadFromStream(in);
    int len = plen.getAsInt();

    mProofChain.reserve(len);
    for (int i = 0; i < len; ++i) {
        MMRProofChunk chunk;
        chunk.readDataStream(in);
        addProofChunk(chunk);
    }
}

MMRProof MMRProof::ReadFromStream(std::istream& in) {
    MMRProof proof;
    proof.readDataStream(in);
    return proof;
}

MMRProof MMRProof::convertMiniDataVersion(const org::minima::objects::base::MiniData& zMMRProof) {
    const std::vector<std::uint8_t>& bytes = zMMRProof.getBytes();
    std::string s(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::istringstream iss(s, std::ios::binary);

    MMRProof proof = MMRProof::ReadFromStream(iss);
    return proof;
}

} // namespace mmr
} // namespace objects
} // namespace minima
} // namespace org