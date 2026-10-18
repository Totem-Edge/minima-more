#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <filesystem>
#include <ostream>
#include <istream>

#include "org/minima/utils/streamable.hpp"
// We must include MMRData here because we return it by value (needs complete type)
#include "org/minima/objects/mmr/m_m_r_data.hpp"

// Namespaced forward declarations (Pitfall 10)
namespace org { namespace minima { namespace objects {
class Coin;
class TxBlock;
} } }

namespace org { namespace minima { namespace objects { namespace base {
class MiniNumber;
} } } }

namespace org { namespace minima { namespace objects { namespace mmr {
class MMR;
class MMREntry;
class MMRProof;
class MMREntryNumber;
} } } }

namespace org {
namespace minima {
namespace objects {
namespace mmr {

class MegaMMR : public org::minima::utils::Streamable {
public:
    MegaMMR();

    // Destructor and move semantics (Pitfall 1)
    virtual ~MegaMMR();
    MegaMMR(MegaMMR&&) noexcept;
    MegaMMR& operator=(MegaMMR&&) noexcept;

    // Delete copy operations
    MegaMMR(const MegaMMR&) = delete;
    MegaMMR& operator=(const MegaMMR&) = delete;

    // Accessors
    org::minima::objects::mmr::MMR& getMMR();
    const org::minima::objects::mmr::MMR& getMMR() const;

    std::unordered_map<std::string, std::unique_ptr<org::minima::objects::Coin>>& getAllCoins();
    const std::unordered_map<std::string, std::unique_ptr<org::minima::objects::Coin>>& getAllCoins() const;

    bool isEmpty() const;

    // Convert the TxBlock
    void addBlock(const org::minima::objects::TxBlock& zBlock);

    // Wipe the data
    void clear();

    // Persistence
    void loadMMR(const std::filesystem::path& zFile);
    void saveMMR(const std::filesystem::path& zFile);

    // Streamable
    void writeDataStream(std::ostream& zOut) override;
    void readDataStream(std::istream& zIn) override;

    // Static helpers (return by value to mirror Java)
    static org::minima::objects::mmr::MMRData getCoinData();
    static org::minima::objects::mmr::MMRData
    getCoinData(const org::minima::objects::base::MiniNumber& zNumber);

private:
    // Helpers
    bool isPrunable(const org::minima::objects::Coin& zCoin) const;
    void scanUnspendable();
    void pruneUnspendable(bool zScanMMR);

    // Members
    std::unique_ptr<org::minima::objects::mmr::MMR> mMMR;
    std::unordered_map<std::string, std::unique_ptr<org::minima::objects::Coin>> mAllUnspentCoins;

    // Logging toggle (Java: private static boolean)
    static bool s_PRUNE_LOGS;
};

} // namespace mmr
} // namespace objects
} // namespace minima
} // namespace org