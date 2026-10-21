#pragma once

#include <string>

namespace minima::analytics {

struct ApiOptions {
    std::string dataDir;
    std::string archivePath;
    std::string mmrPath;
    std::string indexPath;
    bool forceArchive = false;
    bool forceMmr = false;
    int port = 8080;
};

class ApiServer {
public:
    explicit ApiServer(ApiOptions options);
    int run();

private:
    ApiOptions mOptions;
};

} // namespace minima::analytics
