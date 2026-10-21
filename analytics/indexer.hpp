#pragma once

#include <string>

namespace minima::analytics {

struct IndexOptions {
    std::string archivePath;
    std::string indexPath;
    std::string schemaPath;
    bool rebuild = false;
};

struct IndexResult {
    bool ok = false;
    long long blocks = 0;
    std::string error;
};

IndexResult buildIndex(const IndexOptions& options);

} // namespace minima::analytics
