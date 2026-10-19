#include "org/minima/system/commands/base/capabilities.hpp"

#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

capabilities::capabilities()
    : Command("capabilities",
              "Get the node capability manifest (commands, params, annotations)") {}

std::string capabilities::getFullHelp() const {
    return std::string(
        "\ncapabilities\n"
        "\n"
        "Returns the capability manifest generated from the command registry\n"
        "(RFC-0011): node header plus every command with its category, access\n"
        "annotations and parameter specs. This is the single source of truth\n"
        "for the in-node MCP tools/list and the generated docs reference.\n"
        "\n"
        "capabilities                 - JSON manifest\n"
        "capabilities markdown:true   - generated Markdown reference\n"
        "\n");
}

std::vector<std::string> capabilities::getValidParams() const {
    return {"markdown"};
}

std::vector<ParamSpec> capabilities::getParamSpecs() const {
    ParamSpec m;
    m.name = "markdown";
    m.type = "boolean";
    m.required = false;
    m.defaultValue = "false";
    m.description = "Return the generated Markdown command reference instead of JSON";
    return {m};
}

CommandAnnotations capabilities::getAnnotations() const {
    CommandAnnotations a;
    a.readOnly = true;
    a.destructive = false;
    a.idempotent = true;
    a.category = "base";
    return a;
}

std::unique_ptr<org::minima::utils::json::JSONObject> capabilities::runCommand() {
    if (getBooleanParam("markdown", false)) {
        auto ret = getJSONReply();
        ret->put("markdown", org::minima::system::commands::CommandRunner::capabilityManifestMarkdown());
        return ret;
    }
    return org::minima::system::commands::CommandRunner::capabilityManifest();
}

org::minima::system::commands::Command* capabilities::getFunction() {
    return new capabilities();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org
