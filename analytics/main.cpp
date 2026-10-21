#include "api_server.hpp"
#include "indexer.hpp"
#include "backup.hpp"
#include "org/minima/objects/address.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/kissvm/contract.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
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
        << "  -index           Build/update the DuckDB analytics index\n"
        << "  -indexonly       Build the index and exit\n"
        << "  -indexdb FILE    DuckDB index path (default <data>/analytics.duckdb)\n"
        << "  -rebuild         Rebuild the index from scratch\n"
        << "  -mmr-enrich      Load MegaMMR and write root/consistency snapshots\n"
        << "  -mmrstore DIR    Coin LMDB store (default <data>/1.0/databases/coins.lmdb)\n"
        << "  -knownscripts F  CSV of known script labels (default analytics/known_scripts.csv)\n"
        << "  -backup DIR      Export a portable Parquet backup and exit\n"
        << "  -restore DIR     Import a backup into a new -indexdb, then continue\n"
        << "  -rawdata FILE    Read blocks from a raw archive if the archive DB is empty\n"
        << "  -hashscript FILE Compute address/hash for a KISSVM script\n"
        << "  -hashscript-clean FILE  Clean the script first (newscript behaviour)\n"
        << "  -help            Show this help\n";
}

} // namespace

int main(int argc, char** argv) {
    minima::analytics::ApiOptions opt;
    opt.dataDir = homeDir() + "/.minima";
    opt.indexPath = opt.dataDir + "/analytics.duckdb";
    std::string hashScript;
    std::string backupDir;
    std::string restoreDir;
    bool hashClean = false;
    std::string knownScriptsPath = MINIMA_ANALYTICS_KNOWN_SCRIPTS;
    std::string rawPath;
    bool doIndex = false;
    bool indexOnly = false;
    bool rebuild = false;
    bool enrichMmr = false;
    bool mmrStoreSpecified = false;
    std::string mmrStorePath;

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
        } else if (arg == "-indexdb") {
            if (!next(opt.indexPath)) {
                std::cerr << "-indexdb requires a value\n";
                return 2;
            }
        } else if (arg == "-index") {
            doIndex = true;
        } else if (arg == "-indexonly") {
            doIndex = true;
            indexOnly = true;
        } else if (arg == "-rawdata") {
            if (!next(rawPath)) {
                std::cerr << "-rawdata requires a file\n";
                return 2;
            }
        } else if (arg == "-knownscripts") {
            if (!next(knownScriptsPath)) {
                std::cerr << "-knownscripts requires a file\n";
                return 2;
            }
        } else if (arg == "-backup") {
            if (!next(backupDir)) {
                std::cerr << "-backup requires a directory\n";
                return 2;
            }
        } else if (arg == "-restore") {
            if (!next(restoreDir)) {
                std::cerr << "-restore requires a directory\n";
                return 2;
            }
        } else if (arg == "-hashscript" || arg == "-hashscript-clean") {
            hashClean = (arg == "-hashscript-clean");
            if (!next(hashScript)) {
                std::cerr << "-hashscript requires a file\n";
                return 2;
            }
        } else if (arg == "-rebuild") {
            rebuild = true;
        } else if (arg == "-mmr-enrich") {
            enrichMmr = true;
        } else if (arg == "-mmrstore") {
            mmrStoreSpecified = true;
            if (!next(mmrStorePath)) {
                std::cerr << "-mmrstore requires a value\n";
                return 2;
            }
            if (mmrStorePath == "none") mmrStorePath.clear();
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
    if (!mmrStoreSpecified) {
        const std::string a = opt.dataDir + "/1.0/databases/coins.lmdb";
        const std::string b = opt.dataDir + "/databases/coins.lmdb";
        std::error_code ec;
        mmrStorePath = std::filesystem::exists(a, ec) ? a :
                       (std::filesystem::exists(b, ec) ? b : "");
    }
    if (!hashScript.empty()) {
        std::ifstream in(hashScript, std::ios::binary);
        if (!in) {
            std::cerr << "cannot open " << hashScript << "\n";
            return 2;
        }
        std::ostringstream src;
        src << in.rdbuf();
        std::string source = src.str();
        if (hashClean) {
            source = org::minima::kissvm::Contract::cleanScript(source);
        }
        org::minima::objects::Address addr(source);
        std::cout << addr.getAddressData().to0xString() << "\n" << addr.getMinimaAddress() << "\n";
        return 0;
    }

    if (opt.port <= 0 || opt.port > 65535) {
        std::cerr << "Invalid port\n";
        return 2;
    }

    if (!restoreDir.empty()) {
        minima::analytics::BackupOptions bopt;
        bopt.indexPath = opt.indexPath;
        bopt.backupDir = restoreDir;
        auto br = minima::analytics::importBackup(bopt);
        if (!br.ok) {
            std::cerr << "restore failed: " << br.error << "\n";
            return 1;
        }
        std::cerr << "restored index at height " << br.height << "\n";
    }

    if (doIndex) {
        minima::analytics::IndexOptions iopt;
        iopt.archivePath = opt.archivePath;
        iopt.indexPath = opt.indexPath;
        iopt.schemaPath = MINIMA_ANALYTICS_SCHEMA;
        iopt.mmrPath = opt.mmrPath;
        iopt.mmrStorePath = mmrStorePath;
        iopt.knownScriptsPath = knownScriptsPath;
        iopt.rawPath = rawPath;
        iopt.enrichMmr = enrichMmr;
        iopt.rebuild = rebuild;
        auto ir = minima::analytics::buildIndex(iopt);
        if (!ir.ok) {
            std::cerr << "index failed: " << ir.error << "\n";
            return 1;
        }
        std::cerr << "indexed " << ir.blocks << " blocks into " << opt.indexPath << "\n";
        if (indexOnly) return 0;
    }

    if (!backupDir.empty()) {
        minima::analytics::BackupOptions bopt;
        bopt.indexPath = opt.indexPath;
        bopt.backupDir = backupDir;
        auto br = minima::analytics::exportBackup(bopt);
        if (!br.ok) {
            std::cerr << "backup failed: " << br.error << "\n";
            return 1;
        }
        std::cerr << "backup written to " << backupDir
                  << " at height " << br.height << "\n";
        return 0;
    }

    minima::analytics::ApiServer server(opt);
    return server.run();
}
