#pragma once

#include <memory>
#include <ostream>
#include <istream>

#include "org/minima/utils/streamable.hpp"

// Forward declarations for project classes (namespaced as required)
namespace org { namespace minima { namespace objects { namespace mmr { class MegaMMR; } } } }
namespace org { namespace minima { namespace objects { class IBD; } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {
namespace mmrsync {

class MegaMMRBackup : public org::minima::utils::Streamable {
public:
    MegaMMRBackup();
    MegaMMRBackup(org::minima::objects::mmr::MegaMMR* zMMR,
                  org::minima::objects::IBD* zIDB);
    ~MegaMMRBackup() override;

    // Non-owning setters (caller manages lifetime)
    void setMegaMMR(org::minima::objects::mmr::MegaMMR* zMMR);
    void setIBD(org::minima::objects::IBD* zIDB);

    // Accessors (non-owning)
    org::minima::objects::mmr::MegaMMR* getMegaMMR();
    org::minima::objects::IBD* getIBD();

    // Streamable interface using standard C++ streams
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Read a legacy raw MegaMMR file (a Java node's megammr.mmr): version +
    // MMR + coins, with no backup envelope and no IBD.
    void readLegacyMegaMMR(std::istream& in);

private:
    // Pointers are non-owning for export (DB objects); for import they point at
    // the owned instances below (mirrors Java's readDataStream allocation).
    org::minima::objects::mmr::MegaMMR* mMegaMMR;
    org::minima::objects::IBD* mIBD;
    std::unique_ptr<org::minima::objects::mmr::MegaMMR> mOwnedMMR;
    std::unique_ptr<org::minima::objects::IBD> mOwnedIBD;
};

} // namespace mmrsync
} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org