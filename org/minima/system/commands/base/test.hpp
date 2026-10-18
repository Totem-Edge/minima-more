#pragma once

#include <memory>
#include <string>
#include <vector>
#include <iosfwd>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class test : public org::minima::system::commands::Command {
public:
    test();
    virtual ~test() = default;

    // Parameter specification
    std::vector<std::string> getValidParams() const;

    // Execute the command
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Return a new instance of this command
    org::minima::system::commands::Command* getFunction() override;

private:
    // Helper: get a file stream similar to Java's ClassLoader.getResourceAsStream
    std::unique_ptr<std::istream> getFileFromResourceAsStream(const std::string& fileName);
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org