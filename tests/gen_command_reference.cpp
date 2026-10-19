/**
 * gen_command_reference.cpp  (RFC-0011 §9, Phase F)
 *
 * Writes the generated command/tool reference (from the capability manifest) to
 * a file. Usage: gen_command_reference [COMMAND_REFERENCE.md]
 */

#include <fstream>
#include <iostream>
#include <string>

#include "org/minima/system/commands/command_runner.hpp"

int main(int argc, char** argv) {
    const std::string path = (argc > 1) ? argv[1] : "COMMAND_REFERENCE.md";
    const std::string md =
        org::minima::system::commands::CommandRunner::capabilityManifestMarkdown();

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "cannot write " << path << "\n";
        return 1;
    }
    out << md;
    out.close();
    std::cout << "wrote " << path << " (" << md.size() << " bytes)\n";
    return out ? 0 : 1;
}
