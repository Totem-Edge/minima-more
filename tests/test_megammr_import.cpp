/**
 * test_megammr_import.cpp
 *
 * Verifies the two MegaMMR file forms supported by `megammr action:import`:
 *   1. the versioned MegaMMRBackup envelope (Minima More export)
 *   2. a legacy raw MegaMMR (a Java node's megammr.mmr)
 *
 * The old reader threw unless the MegaMMR/IBD were pre-set, and the command
 * loaded through loadObjectSlow() which swallowed the error - so every import
 * silently failed. This pins the fix.
 */

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "org/minima/objects/i_b_d.hpp"
#include "org/minima/objects/mmr/mega_m_m_r.hpp"
#include "org/minima/system/commands/backup/mmrsync/mega_m_m_r_backup.hpp"

using org::minima::objects::IBD;
using org::minima::objects::mmr::MegaMMR;
using org::minima::system::commands::backup::mmrsync::MegaMMRBackup;

static int g_pass = 0, g_total = 0;
static void check(bool ok, const std::string& what) {
    ++g_total;
    if (ok) ++g_pass;
    std::cout << "  " << (ok ? "ok  " : "FAIL") << " " << what << "\n";
}

int main(int argc, char** argv) {
    // Helper: extract the MegaMMR portion of a versioned backup into a legacy
    // raw file (used to exercise `megammr action:import` with a legacy file).
    if (argc == 4 && std::string(argv[1]) == "--extract-raw") {
        std::ifstream in(argv[2], std::ios::binary);
        if (!in) { std::cerr << "cannot open " << argv[2] << "\n"; return 1; }
        MegaMMRBackup b;
        b.readDataStream(in);
        std::ofstream out(argv[3], std::ios::binary | std::ios::trunc);
        if (!out) { std::cerr << "cannot write " << argv[3] << "\n"; return 1; }
        b.getMegaMMR()->writeDataStream(out);
        std::cout << "wrote legacy raw " << argv[3] << "\n";
        return 0;
    }

    std::cout << "=== MegaMMR import formats ===\n\n";

    // 1. Versioned envelope: write from DB objects, read into a fresh backup.
    {
        MegaMMR mmr;
        IBD ibd;
        MegaMMRBackup out(&mmr, &ibd);
        std::ostringstream oss(std::ios::binary);
        out.writeDataStream(oss);

        std::istringstream iss(oss.str(), std::ios::binary);
        MegaMMRBackup in;
        bool threw = false;
        try {
            in.readDataStream(iss);
        } catch (const std::exception& e) {
            threw = true;
            std::cout << "    exception: " << e.what() << "\n";
        }
        check(!threw, "versioned backup reads without pre-set objects");
        check(in.getMegaMMR() != nullptr, "versioned backup allocates its MegaMMR");
        check(in.getIBD() != nullptr, "versioned backup allocates its IBD");
    }

    // 2. Legacy raw MegaMMR (no envelope, no IBD).
    {
        MegaMMR mmr;
        std::ostringstream oss(std::ios::binary);
        mmr.writeDataStream(oss);

        std::istringstream iss(oss.str(), std::ios::binary);
        MegaMMRBackup legacy;
        bool threw = false;
        try {
            legacy.readLegacyMegaMMR(iss);
        } catch (const std::exception& e) {
            threw = true;
            std::cout << "    exception: " << e.what() << "\n";
        }
        check(!threw, "legacy raw MegaMMR reads");
        check(legacy.getMegaMMR() != nullptr, "legacy raw MegaMMR is allocated");
        check(legacy.getIBD() == nullptr, "legacy raw MegaMMR has no IBD");
    }

    std::cout << "\n=== " << g_pass << "/" << g_total << " checks passed ===\n";
    return (g_pass == g_total) ? 0 : 1;
}
