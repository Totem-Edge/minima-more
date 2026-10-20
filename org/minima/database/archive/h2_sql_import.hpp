#pragma once

#include <filesystem>

namespace org {
namespace minima {
namespace database {
namespace archive {

// Result of converting a legacy H2 archive dump to the raw archive format.
struct H2SqlImportResult {
    int blocks = 0;
    bool cascade = false;
    long long firstBlock = -1;
    long long lastBlock = -1;
};

// Convert a legacy H2 `SCRIPT TO ... COMPRESSION GZIP` archive dump into a
// raw archive file (gzip: MiniByte cascade + Cascade + count + TxBlocks) that
// RawArchiveInput can consume. This is a focused reader for the archive schema
// (SYNCBLOCK / CASCADEDATA), not a general H2 engine.
//
// Throws std::runtime_error on malformed input.
H2SqlImportResult h2SqlToRawArchive(const std::filesystem::path& zH2Gzip,
                                    const std::filesystem::path& zRawGzipOut);

} // namespace archive
} // namespace database
} // namespace minima
} // namespace org
