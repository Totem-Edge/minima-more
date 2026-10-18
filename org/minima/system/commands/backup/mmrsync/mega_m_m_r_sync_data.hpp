#pragma once

#include <memory>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include "org/minima/utils/streamable.hpp"

namespace org {
namespace minima {
namespace objects {
namespace base {
class MiniData; // forward declaration
} // namespace base
} // namespace objects
} // namespace minima
} // namespace org

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {
namespace mmrsync {

class MegaMMRSyncData final : public org::minima::utils::Streamable {
public:
    MegaMMRSyncData();
    MegaMMRSyncData(std::vector<std::unique_ptr<org::minima::objects::base::MiniData>> zAllAddresses,
                    std::vector<std::unique_ptr<org::minima::objects::base::MiniData>> zAllPublicKeys);

    // Non-copyable (vector<unique_ptr<...>>)
    MegaMMRSyncData(const MegaMMRSyncData&) = delete;
    MegaMMRSyncData& operator=(const MegaMMRSyncData&) = delete;

    // Move operations (must be declared due to forward-declared MiniData in unique_ptr)
    MegaMMRSyncData(MegaMMRSyncData&&) noexcept;
    MegaMMRSyncData& operator=(MegaMMRSyncData&&) noexcept;

    // Destructor must be declared (PIMPL fix for unique_ptr to incomplete type)
    ~MegaMMRSyncData();

    // Accessors (throw if underlying list not initialized to mimic Java's possible NPE)
    std::vector<std::unique_ptr<org::minima::objects::base::MiniData>>& getAllAddresses();
    const std::vector<std::unique_ptr<org::minima::objects::base::MiniData>>& getAllAddresses() const;

    std::vector<std::unique_ptr<org::minima::objects::base::MiniData>>& getAllPublicKeys();
    const std::vector<std::unique_ptr<org::minima::objects::base::MiniData>>& getAllPublicKeys() const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

private:
    // Optional containers to preserve Java's potential null semantics on default construction
    std::unique_ptr<std::vector<std::unique_ptr<org::minima::objects::base::MiniData>>> mAllAddresses;
    std::unique_ptr<std::vector<std::unique_ptr<org::minima::objects::base::MiniData>>> mAllPublicKeys;
};

} // namespace mmrsync
} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org