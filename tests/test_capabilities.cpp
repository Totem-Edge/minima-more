/**
 * test_capabilities.cpp  (RFC-0011 §5, Phases A-C)
 *
 * Asserts the capability manifest is generated from the command registry and
 * describes every registered command with params + annotations.
 */

#include <iostream>
#include <memory>
#include <set>
#include <string>

#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

using org::minima::system::commands::CommandRunner;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;

static int g_pass = 0, g_total = 0;
static void check(bool ok, const std::string& what) {
    ++g_total;
    if (ok) ++g_pass;
    std::cout << "  " << (ok ? "ok  " : "FAIL") << " " << what << "\n";
}

static bool objOf(const std::any& v, JSONObject& out) {
    if (const auto* o = std::any_cast<JSONObject>(&v)) { out = *o; return true; }
    if (const auto* sp = std::any_cast<std::shared_ptr<JSONObject>>(&v)) {
        if (*sp) { out = **sp; return true; }
    }
    return false;
}

int main() {
    std::cout << std::unitbuf;
    std::cout << "=== capability manifest (RFC-0011 §5) ===\n\n";

    auto m = CommandRunner::capabilityManifest();
    check(m != nullptr, "manifest generated");
    if (!m) {
        std::cout << "\n=== " << g_pass << "/" << g_total << " checks passed ===\n";
        return 1;
    }
    check(m->containsKey("node") && m->containsKey("commands"), "node header + commands present");

    JSONObject node;
    check(objOf(m->get("node"), node), "node is an object");
    check(node.containsKey("version") && node.containsKey("network") &&
          node.containsKey("mode") && node.containsKey("serves"),
          "node header has version/network/mode/serves");

    const auto* cmds = std::any_cast<JSONArray>(&m->get("commands"));
    check(cmds != nullptr && cmds->size() > 0, "commands is a non-empty array");

    std::set<std::string> names;
    bool shape = true, hasStatus = false, hasCaps = false;
    if (cmds) {
        for (const auto& e : cmds->elements()) {
            JSONObject c;
            if (!objOf(e, c)) { shape = false; continue; }
            names.insert(c.getString("name"));
            if (c.getString("name") == "status") hasStatus = true;
            if (c.getString("name") == "capabilities") hasCaps = true;
            if (!c.containsKey("summary") || !c.containsKey("category") ||
                !c.containsKey("annotations") || !c.containsKey("params")) {
                shape = false;
            }
            JSONObject ann;
            if (!objOf(c.get("annotations"), ann) ||
                !ann.containsKey("readOnly") || !ann.containsKey("destructive") ||
                !ann.containsKey("idempotent")) {
                shape = false;
            }
        }
    }
    check(hasStatus, "status is described");
    check(hasCaps, "capabilities is described");
    check(shape, "every command carries summary/category/annotations/params");

    bool all = true;
    for (auto& p : CommandRunner::getCommandPrototypes()) {
        if (!p) continue;
        if (names.find(p->getName()) == names.end()) { all = false; break; }
    }
    check(all, "every registered command is described");

    const std::string md = CommandRunner::capabilityManifestMarkdown();
    check(!md.empty(), "markdown reference generated");
    check(md.find("GENERATED from the node capability manifest") != std::string::npos,
          "markdown carries the generated banner");

    std::cout << "\n=== " << g_pass << "/" << g_total << " checks passed ===\n";
    return (g_pass == g_total) ? 0 : 1;
}
