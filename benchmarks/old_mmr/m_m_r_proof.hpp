#pragma once

#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include <istream>
#include <ostream>

#include "org/minima/utils/streamable.hpp"
#include "org/minima/objects/base/mini_byte.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

// Namespaced forward declarations (Pitfall 10)
namespace org { namespace minima { namespace objects { namespace mmr { class MMRData; } } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; } } } }

namespace org {
namespace minima {
namespace objects {
namespace mmr {

class MMRProof : public org::minima::utils::Streamable {
public:
    class MMRProofChunk : public org::minima::utils::Streamable {
    public:
        MMRProofChunk();
        MMRProofChunk(bool zIsLeft, const org::minima::objects::mmr::MMRData& zData);

        // Special members to handle unique_ptr to forward-declared type (PIMPL fix)
        ~MMRProofChunk();
        MMRProofChunk(MMRProofChunk&&) noexcept;
        MMRProofChunk& operator=(MMRProofChunk&&) noexcept;
        

        // Copy operations (deep copy using Clone)
        MMRProofChunk(const MMRProofChunk& other);
        MMRProofChunk& operator=(const MMRProofChunk& other);

        bool isLeft() const;
        const org::minima::objects::mmr::MMRData& getMMRData() const;

        org::minima::utils::json::JSONObject toJSON() const;

        // Streamable
        void writeDataStream(std::ostream& out) override;
        void readDataStream(std::istream& in) override;

    private:
        org::minima::objects::base::MiniByte mLeft;
        std::unique_ptr<org::minima::objects::mmr::MMRData> mMMRData;
    };

    MMRProof();
    explicit MMRProof(const org::minima::objects::base::MiniNumber& zBlockTime);
    MMRProof(const MMRProof& zOther);

    const org::minima::objects::base::MiniNumber& getBlockTime() const;

    void addProofChunk(const MMRProofChunk& zChunk);
    void addProofChunk(bool zIsLeft, const org::minima::objects::mmr::MMRData& zData);

    MMRProofChunk& getProofChunk(int zProofIndex);
    const MMRProofChunk& getProofChunk(int zProofIndex) const;

    int getProofLength() const;

    // Returns a newly computed MMRData for the proof result
    std::unique_ptr<org::minima::objects::mmr::MMRData>
    calculateProof(const org::minima::objects::mmr::MMRData& zData) const;

    org::minima::utils::json::JSONObject toJSON() const;
    std::string toString() const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    static MMRProof ReadFromStream(std::istream& in);

    // Convert a MiniData version into an MMRProof
    static MMRProof convertMiniDataVersion(const org::minima::objects::base::MiniData& zMMRProof);

private:
    org::minima::objects::base::MiniNumber mBlockTime;
    std::vector<MMRProofChunk> mProofChain;
};

} // namespace mmr
} // namespace objects
} // namespace minima
} // namespace org