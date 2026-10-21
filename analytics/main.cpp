#include "api_server.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

std::string homeDir() {
    const char* home = std::getenv("HOME");
    return home ? std::string(home) : std::string(".");
}

std::string firstExisting(const std::string& base,
                          const std::initializer_list<std::string>& candidates) {
    for (const auto& rel : candidates) {
        const std::string path = base + rel;
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) return path;
    }
    return base + *candidates.begin();
}

void printHelp() {
    std::cout
        << "minima-analytics — mode-aware analytics sidecar\n\n"
        << "Usage: minima-analytics [-data DIR] [-apiport PORT]\n"
        << "                        [-archivepath FILE] [-mmrpath FILE]\n"
        << "                        [-archive] [-megammr]\n\n"
        << "  -data DIR        Node data folder (default ~/.minima)\n"
        << "  -apiport PORT    HTTP API port (default 8080)\n"
        << "  -archivepath F   Override archive SQLite DB path\n"
        << "  -mmrpath F       Override megammr.mmr path\n"
        << "  -archive         Force archive mode (even if DB not found yet)\n"
        << "  -megammr         Force MegaMMR mode\n"
        << "  -help            Show this help\n";
}

} // namespace

int main(int argc, char** argv) {
    minima::analytics::ApiOptions opt;
    opt.dataDir = homeDir() + "/.minima";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&](std::string& out) -> bool {
            if (i + 1 >= argc) return false;
            out = argv[++i];
            return true;
        };
        if (arg == "-help" || arg == "--help" || arg == "-h") {
            printHelp();
            return 0;
        } else if (arg == "-data") {
            if (!next(opt.dataDir)) {
                std::cerr << "-data requires a value\n";
                return 2;
            }
        } else if (arg == "-apiport") {
            std::string value;
            if (!next(value)) {
                std::cerr << "-apiport requires a value\n";
                return 2;
            }
            opt.port = std::stoi(value);
        } else if (arg == "-archivepath") {
            if (!next(opt.archivePath)) {
                std::cerr << "-archivepath requires a value\n";
                return 2;
            }
        } else if (arg == "-mmrpath") {
            if (!next(opt.mmrPath)) {
                std::cerr << "-mmrpath requires a value\n";
                return 2;
            }
        } else if (arg == "-archive") {
            opt.forceArchive = true;
        } else if (arg == "-megammr") {
            opt.forceMmr = true;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            printHelp();
            return 2;
        }
    }

    if (opt.archivePath.empty()) {
        opt.archivePath = firstExisting(opt.dataDir, {
            "/1.0/databases/archivesql/archive",
            "/databases/archivesql/archive",
            "/archive"
        });
    }
    if (opt.mmrPath.empty()) {
        opt.mmrPath = firstExisting(opt.dataDir, {
            "/1.0/databases/megammr.mmr",
            "/megammr.mmr"
        });
    }
    if (opt.port <= 0 || opt.port > 65535) {
        std::cerr << "Invalid port\n";
        return 2;
    }

    minima::analytics::ApiServer server(opt);
    return server.run();
}
