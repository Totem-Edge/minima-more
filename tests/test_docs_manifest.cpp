/**
 * test_docs_manifest.cpp  (RFC-0011 §9, Phase F)
 *
 * Asserts the committed command/tool reference matches the manifest generated
 * from the registry, so the docs cannot drift from the code. Regenerate with
 * `gen_command_reference` when the command surface changes.
 *
 * Usage: test_docs_manifest <path-to-COMMAND_REFERENCE.md>
 */

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "org/minima/system/commands/command_runner.hpp"

static int g_pass = 0, g_total = 0;
static void check(bool ok, const std::string& what) {
    ++g_total;
    if (ok) ++g_pass;
    std::cout << "  " << (ok ? "ok  " : "FAIL") << " " << what << "\n";
}

int main(int argc, char** argv) {
    std::cout << "=== docs <-> capability manifest (RFC-0011 §9) ===\n\n";

    const std::string path = (argc > 1) ? argv[1] : "COMMAND_REFERENCE.md";

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        check(false, std::string("reference file present: ") + path);
        std::cout << "\n=== " << g_pass << "/" << g_total << " checks passed ===\n";
        return 1;
    }
    std::string committed((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const std::string generated =
        org::minima::system::commands::CommandRunner::capabilityManifestMarkdown();

    check(!generated.empty(), "manifest markdown generated");
    check(committed == generated,
          "committed reference matches the manifest (regenerate with gen_command_reference)");

    if (committed != generated) {
        std::size_t i = 0;
        while (i < committed.size() && i < generated.size() && committed[i] == generated[i]) ++i;
        std::cout << "  first difference at byte " << i << "\n";
        std::cout << "    committed: " << committed.substr(i, 60) << "\n";
        std::cout << "    generated: " << generated.substr(i, 60) << "\n";
    }

    std::cout << "\n=== " << g_pass << "/" << g_total << " checks passed ===\n";
    return (g_pass == g_total) ? 0 : 1;
}
