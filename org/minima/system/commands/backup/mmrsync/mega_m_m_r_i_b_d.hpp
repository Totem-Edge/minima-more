#pragma once

#include <memory>
#include <string>
#include <vector>
#include <ostream>
#include <istream>

#include "org/minima/utils/streamable.hpp"

// Namespaced forward declarations to prevent circular includes (Rule 10)
namespace org { namespace minima { namespace objects { class IBD; } } }
namespace org { namespace minima { namespace objects { class CoinProof; } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {
namespace mmrsync {

class MegaMMRIBD : public org::minima::utils::Streamable {
public:
    MegaMMRIBD();
    MegaMMRIBD(std::shared_ptr<org::minima::objects::IBD> zIBD,
               std::vector<std::unique_ptr<org::minima::objects::CoinProof>> zAllCoinProofs);

    // Rule of 5 - default is fine since we use shared_ptr and vector of unique_ptr
    virtual ~MegaMMRIBD() = default;
    MegaMMRIBD(MegaMMRIBD&&) noexcept = default;
    MegaMMRIBD& operator=(MegaMMRIBD&&) noexcept = default;

    // No copy
    MegaMMRIBD(const MegaMMRIBD&) = delete;
    MegaMMRIBD& operator=(const MegaMMRIBD&) = delete;

    // Accessors (mirror Java semantics)
    org::minima::objects::IBD& getIBD();
    const org::minima::objects::IBD& getIBD() const;

    std::vector<std::unique_ptr<org::minima::objects::CoinProof>>& getAllCoinProofs();
    const std::vector<std::unique_ptr<org::minima::objects::CoinProof>>& getAllCoinProofs() const;

    void setPeersList(const std::string& zPeersList);
    std::string getPeersList() const;

    // Streamable overrides
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

private:
    // NOTE: shared_ptr allows holding incomplete type without requiring destructor definition here
    std::shared_ptr<org::minima::objects::IBD> mInitialIBD;
    std::vector<std::unique_ptr<org::minima::objects::CoinProof>> mAllCoinProofs;
    std::string mPeersList;
};

} // namespace mmrsync
} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org