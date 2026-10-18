#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <memory>

#include "org/minima/system/commands/command.hpp"

// Forward declarations (namespaced) for project dependencies
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }
namespace org { namespace minima { namespace txpowdb { namespace sql { class TxPoWList; } } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {

class decryptbackup : public org::minima::system::commands::Command {
public:
    decryptbackup();

    // Not virtual in base, but provided to match Java API
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Required by base class
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;

private:
    // Helpers mirroring Java private methods
    long long readNextBackup(const std::filesystem::path& zOutput, std::istream& zIn);
    std::unique_ptr<org::minima::txpowdb::sql::TxPoWList> readNextTxPoWList(std::istream& zIn);
};

} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org