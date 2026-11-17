#pragma once

#include <memory>
#include <ostream>
#include <istream>
#include <string>
#include <filesystem>

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

// Self-describing sidecar written next to a MegaMMR backup. The payload is the
// existing version-1/2 stream (MegaMMR + complete IBD); the manifest lets an
// import verify the file before it replaces any live state.
struct RestoreManifest {
    bool        present      = false;
    int         version      = 2;
    std::string network;
    long long   tip_block    = -1;
    std::string tip_hash;
    std::string weight;
    long long   first_block  = -1;
    long long   coins        = 0;
    long long   mmr_entries  = 0;
    long long   cascade_tip  = -1;
    long long   tree_root    = -1;
    long long   tree_tip     = -1;
    long long   ibd_blocks   = 0;
    std::string sha256;
    std::string created_at;
};

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

    // Import helpers: transfer ownership of the parsed objects.
    std::unique_ptr<org::minima::objects::IBD> takeIBD();
    std::unique_ptr<org::minima::objects::mmr::MegaMMR> takeMegaMMR();

    // Streamable interface using standard C++ streams
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Read a legacy raw MegaMMR file (a Java node's megammr.mmr): version +
    // MMR + coins, with no backup envelope and no IBD.
    void readLegacyMegaMMR(std::istream& in);

    // Manifest helpers (plain key=value sidecar, <payload>.manifest).
    static std::string fileSha256(const std::filesystem::path& zFile);
    static RestoreManifest buildManifest(MegaMMRBackup& zBackup,
                                         const std::filesystem::path& zPayload);
    static bool writeManifest(const std::filesystem::path& zFile,
                              const RestoreManifest& zManifest);
    static RestoreManifest readManifest(const std::filesystem::path& zFile);
    // Returns false and fills zError when the payload does not match.
    static bool verifyManifest(const RestoreManifest& zManifest,
                               MegaMMRBackup& zBackup,
                               const std::filesystem::path& zPayload,
                               std::string& zError);

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