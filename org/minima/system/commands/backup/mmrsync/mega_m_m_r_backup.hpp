#pragma once

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

    // Non-owning setters (caller manages lifetime)
    void setMegaMMR(org::minima::objects::mmr::MegaMMR* zMMR);
    void setIBD(org::minima::objects::IBD* zIDB);

    // Accessors (non-owning)
    org::minima::objects::mmr::MegaMMR* getMegaMMR();
    org::minima::objects::IBD* getIBD();

    // Streamable interface using standard C++ streams
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

private:
    // Non-owning raw pointers to avoid incomplete-type destructor issues
    org::minima::objects::mmr::MegaMMR* mMegaMMR;
    org::minima::objects::IBD* mIBD;
};

} // namespace mmrsync
} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org