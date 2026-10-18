#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

#include "org/minima/system/commands/command.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/streamable.hpp"
#include "org/minima/system/commands/backup/mmrsync/mega_m_m_r_sync_data.hpp"
#include "org/minima/system/commands/backup/mmrsync/mega_m_m_r_i_b_d.hpp"

// Forward declarations to avoid heavy includes in header (Pitfall 4)
namespace org { namespace minima { namespace objects {
class IBD;
class CoinProof;
class Coin;
} } }
namespace org { namespace minima { namespace utils { namespace json {
class JSONObject;
class JSONArray;
} } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {
namespace mmrsync {

// class MegaMMRSyncData final : public org::minima::utils::Streamable {
// public:
//     MegaMMRSyncData() = default;
//     MegaMMRSyncData(const std::vector<org::minima::objects::base::MiniData>& zAddresses,
//                     const std::vector<org::minima::objects::base::MiniData>& zPublicKeys);

//     const std::vector<org::minima::objects::base::MiniData>& getAllAddresses() const;
//     const std::vector<org::minima::objects::base::MiniData>& getAllPublicKeys() const;

//     // Streamable
//     void writeDataStream(std::ostream& out) override;
//     void readDataStream(std::istream& in) override;

// private:
//     std::vector<org::minima::objects::base::MiniData> mAllAddresses;
//     std::vector<org::minima::objects::base::MiniData> mAllPublicKeys;
// };

// class MegaMMRIBD final : public org::minima::utils::Streamable {
// public:
//     MegaMMRIBD();
//     MegaMMRIBD(std::unique_ptr<org::minima::objects::IBD> zIBD,
//                std::vector<std::unique_ptr<org::minima::objects::CoinProof>> zAllCoinProofs);

//     // PIMPL-FIX for unique_ptr to forward-declared types
//     ~MegaMMRIBD();
//     MegaMMRIBD(MegaMMRIBD&&) noexcept;
//     MegaMMRIBD& operator=(MegaMMRIBD&&) noexcept;
//     MegaMMRIBD(const MegaMMRIBD&) = delete;
//     MegaMMRIBD& operator=(const MegaMMRIBD&) = delete;

//     org::minima::objects::IBD* getIBD();
//     const org::minima::objects::IBD* getIBD() const;

//     std::vector<std::unique_ptr<org::minima::objects::CoinProof>>& getAllCoinProofs();
//     const std::vector<std::unique_ptr<org::minima::objects::CoinProof>>& getAllCoinProofs() const;

//     void setPeersList(const std::string& zPeersList);
//     std::string getPeersList() const;

//     // Streamable
//     void writeDataStream(std::ostream& out) override;
//     void readDataStream(std::istream& in) override;

// private:
//     std::unique_ptr<org::minima::objects::IBD> mInitialIBD;
//     std::vector<std::unique_ptr<org::minima::objects::CoinProof>> mAllCoinProofs;
//     std::string mPeersList;
// };

class megammrsync final : public org::minima::system::commands::Command {
public:
    megammrsync();

    // Help and params (base methods are not virtual; do not mark override)
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Execute
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;

    // Utilities
    static void updateP2PDB(const std::string& zPeersList);

    static MegaMMRSyncData getMyDetails();

    static std::unique_ptr<MegaMMRIBD> getCurrentMegaMMRIBD(const MegaMMRSyncData& zSyncData);

    static std::vector<std::unique_ptr<org::minima::objects::CoinProof>>
    getAllCoinProofs(const MegaMMRSyncData& zSynData);

    static std::vector<std::unique_ptr<org::minima::objects::Coin>>
    searchMegaCoins(const std::vector<std::unique_ptr<org::minima::objects::base::MiniData>>& zAddresses,
                    const std::vector<std::unique_ptr<org::minima::objects::base::MiniData>>& zPublicKeys);

    // Network
    static std::unique_ptr<MegaMMRIBD> sendMegaMMRSyncReq(const std::string& zHost,
                                                          int zPort,
                                                          const MegaMMRSyncData& zSyncData);
};

} // namespace mmrsync
} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org