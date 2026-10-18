#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>
#include <istream>

#include "org/minima/system/commands/command.hpp"
#include "org/minima/system/commands/command_exception.hpp"

// Forward declarations (namespaced per Pitfall 10)
namespace org { namespace minima { namespace database { class MinimaDB; } } }
namespace org { namespace minima { namespace txpowdb { namespace sql { class TxPoWList; } } } }
namespace org { namespace minima { namespace database { namespace txpowdb { namespace sql { class TxPoWSqlDB; } } } } }
namespace org { namespace minima { namespace objects { class TxPoW; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; class MiniNumber; } } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {

class restoresync : public org::minima::system::commands::Command {
public:
    restoresync();

    // Help and params
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory (clone)
    org::minima::system::commands::Command* getFunction() override;

    // Perform a resync (archive networking not available in provided headers)
    std::unique_ptr<org::minima::utils::json::JSONObject>
    performResync(const std::string& zHost, int zKeyUses,
                  const org::minima::objects::base::MiniNumber& zStartBlock,
                  bool zIncrementKeys);

private:
    // Helpers
    std::uintmax_t readNextBackup(const std::filesystem::path& zOutput, std::istream& zIn);
    std::unique_ptr<org::minima::txpowdb::sql::TxPoWList> readNextTxPoWList(std::istream& zIn);

    // Decrypt and decompress remaining stream into a temporary decompressed file.
    // Throws CommandException on incorrect password or invalid data.
    std::filesystem::path decryptAndDecompressToTemp(const std::vector<std::uint8_t>& zRestoreData,
                                                     const std::filesystem::path& zRestoreFolder,
                                                     const std::string& zPassword);
};

} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org