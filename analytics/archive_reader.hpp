#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

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

using BlockSink = std::function<bool(std::int64_t height, const std::vector<std::uint8_t>& bytes)>;

bool forEachArchiveBlock(const std::string& path, std::int64_t fromHeight,
                         const BlockSink& sink, std::string& error);

} // namespace minima::analytics
