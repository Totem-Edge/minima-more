#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <istream>
#include <sstream> // for std::istringstream

#include "org/minima/system/commands/command.hpp"

// Namespaced forward declarations for project dependencies (Rule 10)
namespace org { namespace minima { namespace txpowdb { namespace sql { class TxPoWList; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {

class restore final : public org::minima::system::commands::Command {
public:
    restore();

    // Help and params
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Run
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;

private:
    // Read a MiniData from zIn and write to zOutput file. Returns the written file size.
    std::int64_t readNextBackup(const std::filesystem::path& zOutput, std::istream& zIn);

    // Read a MiniData representing a TxPoWList and convert it.
    // Returns nullptr on failure.
    std::unique_ptr<org::minima::txpowdb::sql::TxPoWList> readNextTxPoWList(std::istream& zIn);

    // Helper to create a binary istringstream from a byte buffer
    static std::istringstream makeBinaryIStream(const std::vector<std::uint8_t>& data);
};

} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org