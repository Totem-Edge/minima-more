#include "org/minima/system/commands/backup/mmrsync/mega_m_m_r_sync_data.hpp"

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include <ostream>
#include <istream>
#include <utility>

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {
namespace mmrsync {

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;

// Special member functions definitions (after including full MiniData)
MegaMMRSyncData::~MegaMMRSyncData() = default;
MegaMMRSyncData::MegaMMRSyncData(MegaMMRSyncData&&) noexcept = default;
MegaMMRSyncData& MegaMMRSyncData::operator=(MegaMMRSyncData&&) noexcept = default;

MegaMMRSyncData::MegaMMRSyncData() = default;

MegaMMRSyncData::MegaMMRSyncData(std::vector<std::unique_ptr<MiniData>> zAllAddresses,
                                 std::vector<std::unique_ptr<MiniData>> zAllPublicKeys)
{
    mAllAddresses  = std::make_unique<std::vector<std::unique_ptr<MiniData>>>(std::move(zAllAddresses));
    mAllPublicKeys = std::make_unique<std::vector<std::unique_ptr<MiniData>>>(std::move(zAllPublicKeys));
}

std::vector<std::unique_ptr<MiniData>>& MegaMMRSyncData::getAllAddresses() {
    if (!mAllAddresses) {
        throw std::runtime_error("MegaMMRSyncData: mAllAddresses is uninitialized");
    }
    return *mAllAddresses;
}

const std::vector<std::unique_ptr<MiniData>>& MegaMMRSyncData::getAllAddresses() const {
    if (!mAllAddresses) {
        throw std::runtime_error("MegaMMRSyncData: mAllAddresses is uninitialized");
    }
    return *mAllAddresses;
}

std::vector<std::unique_ptr<MiniData>>& MegaMMRSyncData::getAllPublicKeys() {
    if (!mAllPublicKeys) {
        throw std::runtime_error("MegaMMRSyncData: mAllPublicKeys is uninitialized");
    }
    return *mAllPublicKeys;
}

const std::vector<std::unique_ptr<MiniData>>& MegaMMRSyncData::getAllPublicKeys() const {
    if (!mAllPublicKeys) {
        throw std::runtime_error("MegaMMRSyncData: mAllPublicKeys is uninitialized");
    }
    return *mAllPublicKeys;
}

void MegaMMRSyncData::writeDataStream(std::ostream& out) {
    // Emulate Java NullPointerException-like behavior if lists are not set
    if (!mAllAddresses || !mAllPublicKeys) {
        throw std::runtime_error("MegaMMRSyncData: Attempt to serialize with uninitialized lists");
    }

    // Version
    MiniNumber::WriteToStream(out, 1);

    // Addresses
    int len = static_cast<int>(mAllAddresses->size());
    MiniNumber::WriteToStream(out, len);
    for (const auto& cp : *mAllAddresses) {
        if (!cp) {
            throw std::runtime_error("MegaMMRSyncData: Null MiniData in mAllAddresses");
        }
        cp->writeDataStream(out);
    }

    // Public Keys
    len = static_cast<int>(mAllPublicKeys->size());
    MiniNumber::WriteToStream(out, len);
    for (const auto& cp : *mAllPublicKeys) {
        if (!cp) {
            throw std::runtime_error("MegaMMRSyncData: Null MiniData in mAllPublicKeys");
        }
        cp->writeDataStream(out);
    }
}

void MegaMMRSyncData::readDataStream(std::istream& in) {
    // Version (currently unused, but read to maintain wire compatibility)
    int version = MiniNumber::ReadFromStream(in).getAsInt();
    (void)version;

    // Addresses
    mAllAddresses = std::make_unique<std::vector<std::unique_ptr<MiniData>>>();
    int len = MiniNumber::ReadFromStream(in).getAsInt();
    if (len < 0) {
        throw std::runtime_error("MegaMMRSyncData: Negative length for mAllAddresses");
    }
    mAllAddresses->reserve(static_cast<std::size_t>(len));
    for (int i = 0; i < len; ++i) {
        MiniData md = MiniData::ReadFromStream(in);
        mAllAddresses->emplace_back(std::make_unique<MiniData>(std::move(md)));
    }

    // Public Keys
    mAllPublicKeys = std::make_unique<std::vector<std::unique_ptr<MiniData>>>();
    len = MiniNumber::ReadFromStream(in).getAsInt();
    if (len < 0) {
        throw std::runtime_error("MegaMMRSyncData: Negative length for mAllPublicKeys");
    }
    mAllPublicKeys->reserve(static_cast<std::size_t>(len));
    for (int i = 0; i < len; ++i) {
        MiniData md = MiniData::ReadFromStream(in);
        mAllPublicKeys->emplace_back(std::make_unique<MiniData>(std::move(md)));
    }
}

} // namespace mmrsync
} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org