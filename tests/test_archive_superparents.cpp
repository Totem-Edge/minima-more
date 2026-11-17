// Archive super-parent check: a block whose non-zero super-parent is absent
// from the archive (or points at a later height) must be rejected before it is
// stored. This is the check that catches the bad block 2,421,802 which had a
// valid parentID chain but level-6..10 super-parents that were never in the
// archive.
#include "org/minima/database/archive/archive_manager.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_header.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/params/global_params.hpp"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

using org::minima::database::archive::ArchiveManager;
using org::minima::objects::TxBlock;
using org::minima::objects::TxPoW;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) ++failures;
}

std::string seedId(int block) {
    char buf[80];
    std::snprintf(buf, sizeof(buf), "0x%064X", static_cast<unsigned>(block));
    return std::string(buf);
}

TxPoW makePow(int block) {
    TxPoW txpow(seedId(block), block, 100, true, "0x00", false);
    txpow.getTxHeader().mBlockNumber = MiniNumber(block);
    // Real blocks carry 0x00 for every level above their own super-level.
    for (int i = 0; i < org::minima::system::params::GlobalParams::MINIMA_CASCADE_LEVELS; ++i) {
        txpow.setSuperParent(i, MiniData::ZERO_TXPOWID());
    }
    return txpow;
}

} // namespace

int main() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "hawk_archive_superparents_test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    org::minima::system::params::GeneralParams::DATA_FOLDER = dir.string();

    ArchiveManager arch;
    arch.loadDB((dir / "archive").string());

    TxPoW p1 = makePow(1);
    TxBlock b1(p1);
    TxPoW p2 = makePow(2);
    p2.setSuperParent(0, MiniData(seedId(1)));
    TxBlock b2(p2);

    arch.beginSaveBatch();
    arch.saveBlock(b1);
    arch.saveBlock(b2);
    arch.endSaveBatch(true);
    arch.flushPending();
    arch.checkpoint();

    check(arch.exists(seedId(1)).getAsLong() == 1, "block 1 stored");
    check(arch.exists(seedId(2)).getAsLong() == 2, "block 2 stored");
    check(arch.superParentsPresent(b2.getTxPoW(), 2, 1),
          "block 2 super-parent resolves in archive");

    TxPoW good3 = makePow(3);
    good3.setSuperParent(0, MiniData(seedId(2)));
    check(arch.superParentsPresent(good3, 3, 1),
          "block 3 with an existing super-parent is accepted");

    const std::string missing =
        "0x00000000E524F113F46656562C6399555C906E348494EC9587F298261A4E3838";
    TxPoW bad3 = makePow(3);
    bad3.setSuperParent(0, MiniData(missing));
    check(!arch.superParentsPresent(bad3, 3, 1),
          "block 3 with a missing super-parent is rejected");

    TxPoW future = makePow(4);
    future.setSuperParent(0, MiniData(seedId(2)));
    check(arch.superParentsPresent(future, 2, 1) == false,
          "super-parent at or above the block height is rejected");

    arch.hardCloseDB();
    fs::remove_all(dir, ec);

    if (failures != 0) {
        std::cerr << failures << " archive super-parent check(s) failed\n";
        return 1;
    }
    std::cout << "archive super-parent: all checks passed\n";
    return 0;
}
