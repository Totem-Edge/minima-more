#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

// Forward declarations to avoid heavy includes in the header (Pitfall 4)
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

class txnexport final : public org::minima::system::commands::Command {
public:
    txnexport();

    // Help and parameters
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Command API
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;

    ~txnexport() override = default;
    txnexport(txnexport&&) noexcept = default;
    txnexport& operator=(txnexport&&) noexcept = default;

    txnexport(const txnexport&) = delete;
    txnexport& operator=(const txnexport&) = delete;
};

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org