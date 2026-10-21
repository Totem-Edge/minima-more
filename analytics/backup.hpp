#pragma once

#include <string>

namespace minima::analytics {

struct BackupOptions {
    std::string indexPath;
    std::string backupDir;
};

struct BackupResult {
    bool ok = false;
    long long height = -1;
    std::string error;
};

BackupResult exportBackup(const BackupOptions& options);
BackupResult importBackup(const BackupOptions& options);

} // namespace minima::analytics
