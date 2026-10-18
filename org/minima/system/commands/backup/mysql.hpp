#pragma once

#include <string>
#include <vector>
#include <memory>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {

class mysql final : public org::minima::system::commands::Command {
public:
    mysql();
    ~mysql() override = default;

    // Help/params
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Command
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;

    // Utility: parse "user:password@host:port/database" and store login details + auto-backup
    static void convertMySQLParams(const std::string& zMySQLDB);

private:
    // Small helper to format time (milliseconds since epoch) into human-readable string
    static std::string formatDateMillis(long long millis);
};

} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org