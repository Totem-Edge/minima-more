#pragma once

#include <memory>
#include <filesystem>
#include <istream>
#include <cstdint>

// Forward declarations (namespaced per Pitfall 4)
namespace org { namespace minima { namespace database { namespace cascade { class Cascade; } } } }
namespace org { namespace minima { namespace objects { class IBD; class TxBlock; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniByte; class MiniNumber; } } } }

namespace org {
namespace minima {
namespace database {
namespace archive {

class RawArchiveInput {
public:
    // Constructor
    explicit RawArchiveInput(const std::filesystem::path& zFile);

    // Destructor and move operations (PIMPL fix for unique_ptr<Cascade>)
    ~RawArchiveInput();
    RawArchiveInput(RawArchiveInput&&) noexcept;
    RawArchiveInput& operator=(RawArchiveInput&&) noexcept;

    // Delete copy operations
    RawArchiveInput(const RawArchiveInput&) = delete;
    RawArchiveInput& operator=(const RawArchiveInput&) = delete;

    // Open and parse header of the RAW archive (throws on error)
    void connect();

    // Close the underlying streams (idempotent)
    void stop();

    // Access the optional Cascade parsed from the archive header (may be nullptr)
    const org::minima::database::cascade::Cascade* getCascade() const;

    // Retrieve the next IBD chunk (up to 256 blocks). Returns empty IBD (no blocks) if finished.
    std::unique_ptr<org::minima::objects::IBD> getNextIBD();

private:
    std::filesystem::path mFile;

    // Decompressed data stream over the gzip file
    std::unique_ptr<std::istream> mDataIn;

    int mTotalFound {0};
    int mTotalAdded {0};

    // Optional Cascade attached to the archive
    std::unique_ptr<org::minima::database::cascade::Cascade> mCascade;
};

} // namespace archive
} // namespace database
} // namespace minima
} // namespace org