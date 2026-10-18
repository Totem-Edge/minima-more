#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org { namespace minima { namespace system { namespace commands {

class help final : public org::minima::system::commands::Command {
public:
    help();
    ~help() override = default;

    // Parameters list
    std::vector<std::string> getValidParams() const;

    // Execute
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;

private:
    // Add a command's short help into the details JSON
    void addCommand(org::minima::utils::json::JSONObject& zDetails,
                    const org::minima::system::commands::Command& zCommand);

    // Truncate or right-pad with spaces to zDesiredLen
    std::string getStrOfLength(int zDesiredLen, const std::string& zString) const;
};

} } } } // namespace org::minima::system::commands