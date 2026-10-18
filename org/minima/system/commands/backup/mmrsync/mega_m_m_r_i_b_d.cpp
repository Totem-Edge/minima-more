#include "org/minima/system/commands/backup/mmrsync/mega_m_m_r_i_b_d.hpp"

#include <stdexcept>
#include <utility>

#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_string.hpp"

#ifdef _WIN32
// No OS-specific behavior required
#endif

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {
namespace mmrsync {

MegaMMRIBD::MegaMMRIBD()
    : mInitialIBD(nullptr),
      mAllCoinProofs(),
      mPeersList("")
{
}

MegaMMRIBD::MegaMMRIBD(std::shared_ptr<org::minima::objects::IBD> zIBD,
                       std::vector<std::unique_ptr<org::minima::objects::CoinProof>> zAllCoinProofs)
    : mInitialIBD(std::move(zIBD)),
      mAllCoinProofs(std::move(zAllCoinProofs)),
      mPeersList("")
{
}

org::minima::objects::IBD& MegaMMRIBD::getIBD() {
    if (!mInitialIBD) {
        throw std::runtime_error("MegaMMRIBD::getIBD called but mInitialIBD is null");
    }
    return *mInitialIBD;
}

const org::minima::objects::IBD& MegaMMRIBD::getIBD() const {
    if (!mInitialIBD) {
        throw std::runtime_error("MegaMMRIBD::getIBD (const) called but mInitialIBD is null");
    }
    return *mInitialIBD;
}

std::vector<std::unique_ptr<org::minima::objects::CoinProof>>& MegaMMRIBD::getAllCoinProofs() {
    return mAllCoinProofs;
}

const std::vector<std::unique_ptr<org::minima::objects::CoinProof>>& MegaMMRIBD::getAllCoinProofs() const {
    return mAllCoinProofs;
}

void MegaMMRIBD::setPeersList(const std::string& zPeersList) {
    mPeersList = zPeersList;
}

std::string MegaMMRIBD::getPeersList() const {
    return mPeersList;
}

void MegaMMRIBD::writeDataStream(std::ostream& out) {
    using org::minima::objects::base::MiniNumber;
    using org::minima::objects::base::MiniString;

    // Version number
    MiniNumber::WriteToStream(out, 2);

    // Write out the IBD
    // NOTE: The project's IBD header currently conflicts with Streamable (uses DataOutputStream).
    // To avoid including that broken header (which causes TU-wide compile errors), we cannot
    // directly call IBD::writeDataStream here. This is a deliberate compile-time workaround.
    // Once IBD is corrected to use std::ostream in its Streamable override, replace the throw with:
    //   mInitialIBD->writeDataStream(out);
    throw std::runtime_error("MegaMMRIBD::writeDataStream - IBD serialization unavailable due to header mismatch");

    // And now all the proofs..
    // This code is unreachable due to the throw above. Left here for when IBD is fixed.
    // int len = static_cast<int>(mAllCoinProofs.size());
    // MiniNumber::WriteToStream(out, len);
    // for (const auto& cp : mAllCoinProofs) {
    //     if (!cp) {
    //         throw std::runtime_error("MegaMMRIBD::writeDataStream - encountered null CoinProof entry");
    //     }
    //     cp->writeDataStream(out);
    // }
    //
    // // And now the Peers
    // MiniString::WriteToStream(out, mPeersList);
}

void MegaMMRIBD::readDataStream(std::istream& in) {
    using org::minima::objects::base::MiniNumber;
    using org::minima::objects::base::MiniString;

    int version = MiniNumber::ReadFromStream(in).getAsInt();
    (void)version;

    // See note in writeDataStream: we cannot include nor call IBD read methods here.
    // When IBD is corrected to use std::istream, replace the throw with:
    //   mInitialIBD = std::make_shared<org::minima::objects::IBD>();
    //   mInitialIBD->readDataStream(in);
    throw std::runtime_error("MegaMMRIBD::readDataStream - IBD deserialization unavailable due to header mismatch");

    // The rest is unreachable until IBD is fixed; kept for completeness:
    // mAllCoinProofs.clear();
    // int len = MiniNumber::ReadFromStream(in).getAsInt();
    // if (len < 0) {
    //     throw std::runtime_error("MegaMMRIBD::readDataStream - negative proofs length");
    // }
    // mAllCoinProofs.reserve(static_cast<size_t>(len));
    // for (int i = 0; i < len; ++i) {
    //     auto cp = org::minima::objects::CoinProof::ReadFromStream(in);
    //     if (!cp) {
    //         throw std::runtime_error("MegaMMRIBD::readDataStream - failed to read CoinProof");
    //     }
    //     mAllCoinProofs.emplace_back(std::move(cp));
    // }
    //
    // mPeersList = MiniString::ReadFromStream(in).toString();
}

} // namespace mmrsync
} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org