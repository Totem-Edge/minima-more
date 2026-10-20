#include "org/minima/system/commands/backup/mmrsync/mega_m_m_r_backup.hpp"

#include <stdexcept>

#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/i_b_d.hpp"
#include "org/minima/objects/mmr/mega_m_m_r.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {
namespace mmrsync {

MegaMMRBackup::MegaMMRBackup()
    : mMegaMMR(nullptr), mIBD(nullptr) {}

MegaMMRBackup::MegaMMRBackup(
    org::minima::objects::mmr::MegaMMR* zMMR,
    org::minima::objects::IBD* zIDB)
    : mMegaMMR(zMMR), mIBD(zIDB) {}

MegaMMRBackup::~MegaMMRBackup() = default;

void MegaMMRBackup::setMegaMMR(org::minima::objects::mmr::MegaMMR* zMMR) {
    mOwnedMMR.reset();
    mMegaMMR = zMMR;
}

void MegaMMRBackup::setIBD(org::minima::objects::IBD* zIDB) {
    mOwnedIBD.reset();
    mIBD = zIDB;
}

org::minima::objects::mmr::MegaMMR* MegaMMRBackup::getMegaMMR() {
    return mMegaMMR;
}

org::minima::objects::IBD* MegaMMRBackup::getIBD() {
    return mIBD;
}

void MegaMMRBackup::writeDataStream(std::ostream& out) {
    using org::minima::objects::base::MiniNumber;

    // Version
    MiniNumber::WriteToStream(out, 1);

    if (!mMegaMMR || !mIBD) {
        throw std::runtime_error("MegaMMRBackup::writeDataStream called with null MegaMMR or IBD");
    }

    // Upcast to Streamable without including dependent headers
    org::minima::utils::Streamable* mmr_streamable =
        reinterpret_cast<org::minima::utils::Streamable*>(mMegaMMR);
    org::minima::utils::Streamable* ibd_streamable =
        reinterpret_cast<org::minima::utils::Streamable*>(mIBD);

    // Now the Mega MMR
    mmr_streamable->writeDataStream(out);

    // And the IBD
    ibd_streamable->writeDataStream(out);
}

void MegaMMRBackup::readDataStream(std::istream& in) {
    using org::minima::objects::base::MiniNumber;

    int version = MiniNumber::ReadFromStream(in).getAsInt();
    if (version < 1 || version > 8) {
        throw std::runtime_error("Unsupported MegaMMR backup version " + std::to_string(version));
    }

    // Mirror Java: allocate the objects when reading a file.
    if (!mMegaMMR) {
        mOwnedMMR = std::make_unique<org::minima::objects::mmr::MegaMMR>();
        mMegaMMR = mOwnedMMR.get();
    }
    if (!mIBD) {
        mOwnedIBD = std::make_unique<org::minima::objects::IBD>();
        mIBD = mOwnedIBD.get();
    }

    // Upcast to Streamable without including dependent headers
    org::minima::utils::Streamable* mmr_streamable =
        reinterpret_cast<org::minima::utils::Streamable*>(mMegaMMR);
    org::minima::utils::Streamable* ibd_streamable =
        reinterpret_cast<org::minima::utils::Streamable*>(mIBD);

    // Read into provided instances
    mmr_streamable->readDataStream(in);
    ibd_streamable->readDataStream(in);
}

void MegaMMRBackup::readLegacyMegaMMR(std::istream& in) {
    mOwnedMMR = std::make_unique<org::minima::objects::mmr::MegaMMR>();
    mMegaMMR = mOwnedMMR.get();
    mOwnedIBD.reset();
    mIBD = nullptr;
    mMegaMMR->readDataStream(in);
}

} // namespace mmrsync
} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org