#pragma once

#include <cstdint>
#include <string>

namespace minima::analytics {

struct ArchiveStats {
    bool exists = false;
    bool complete = false;
    std::string path;
    std::string error;
    std::int64_t sizeBytes = 0;
    std::int64_t count = 0;
    std::int64_t minBlock = 0;
    std::int64_t maxBlock = 0;
    std::int64_t distinctIds = 0;
    std::int64_t chunks = -1;
    std::int64_t missingChunks = -1;
};

ArchiveStats readArchiveStats(const std::string& path);

} // namespace minima::analytics
