// Archive save/load round-trip regression.
//
// Unlike test_archive_records (which validates an archive produced by a real
// import) and test_archive_tail (which replays a raw archive file), this test
// builds its own archive from synthetic TxBlocks and needs no fixture. It is
// the ctest gate for the archive writer: schema creation, row bookkeeping,
// compressed chunk flush, reopen, duplicate handling and gap detection.
//
// The synthetic blocks are Tagged with isTesting so no mining or signing is
// required; the archive stores serialized data only.
#include "org/minima/database/archive/archive_manager.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_header.hpp"
#include "org/minima/objects/tx_po_w.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include <sqlite3.h>
#include <unistd.h>  // getpid()

using org::minima::database::archive::ArchiveManager;
using org::minima::objects::TxBlock;
using org::minima::objects::TxPoW;

namespace {

int gFailures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) ++gFailures;
}

std::string txpowId(int block) {
    char buf[80];
    std::snprintf(buf, sizeof(buf), "0x%064X", static_cast<unsigned>(block));
    return std::string(buf);
}

TxBlock makeBlock(int block) {
    TxPoW txpow(txpowId(block), block, 100, true, "0x00", false);
    // The test constructor stores the height in test-only state; the header is
    // what the archive serializes, so set it there too or a reload reads 0.
    txpow.getTxHeader().mBlockNumber = org::minima::objects::base::MiniNumber(block);
    return TxBlock(txpow);
}

bool save(ArchiveManager& arch, int block) {
    TxBlock blk = makeBlock(block);
    return arch.saveBlock(blk);
}

long long heightOf(const std::unique_ptr<TxBlock>& blk) {
    if (!blk) return -1;
    return blk->getTxPoW().getBlockNumber().getAsLong();
}

} // namespace

int main(int argc, char** argv) {
    std::filesystem::path dbPath;
    bool temporary = false;
    if (argc > 1) {
        dbPath = argv[1];
        std::filesystem::remove(dbPath);
    } else {
        std::error_code ec;
        dbPath = std::filesystem::temp_directory_path(ec) /
                 ("hawk-archive-roundtrip-" + std::to_string(::getpid()) + ".db");
        if (ec) {
            std::printf("FAIL cannot create temp path\n");
            return 1;
        }
        std::filesystem::remove(dbPath);
        temporary = true;
    }

    sqlite3_initialize();

    {
        ArchiveManager arch;
        arch.loadDB(dbPath.string());

        bool saved = true;
        for (int i = 1; i <= 5; ++i) saved = save(arch, i) && saved;
        check(saved, "saveBlock accepts synthetic blocks");
        arch.flushPending();

        check(arch.getSize() == 5, "five blocks are stored");
        check(heightOf(arch.loadFirstBlock()) == 1, "loadFirstBlock is block 1");
        check(heightOf(arch.loadLastBlock()) == 5, "loadLastBlock is block 5");
        check(arch.firstMissingBlock() == -1, "1..5 is contiguous");
        check(arch.exists(txpowId(3)).getAsLong() == 3, "exists() resolves a stored id");
        check(arch.loadBlock(txpowId(4)) != nullptr, "loadBlock() returns a stored block");

        // Idempotence: re-saving an existing block must not duplicate the row.
        check(save(arch, 3), "re-save of an existing block returns true");
        arch.flushPending();
        check(arch.getSize() == 5, "re-save does not duplicate the row");

        // Gap detection: block 6 missing, block 7 present.
        check(save(arch, 7), "save beyond the tip returns true");
        arch.flushPending();
        check(arch.getSize() == 6, "gap block is stored");
        check(arch.firstMissingBlock() == 6, "firstMissingBlock finds the gap");

        // A batch boundary must not lose the buffered tail.
        arch.beginSaveBatch();
        for (int i = 8; i <= 12; ++i) save(arch, i);
        arch.endSaveBatch(true);
        check(arch.getSize() == 11, "batched blocks are committed");
        check(heightOf(arch.loadLastBlock()) == 12, "batched tip is block 12");
    }

    {
        // Reopen: the schema and rows must survive a fresh ArchiveManager.
        ArchiveManager reopened;
        reopened.loadDB(dbPath.string());
        check(reopened.getSize() == 11, "size survives reopen");
        check(heightOf(reopened.loadFirstBlock()) == 1, "first block survives reopen");
        check(heightOf(reopened.loadLastBlock()) == 12, "tip survives reopen");
        check(reopened.exists(txpowId(12)).getAsLong() == 12, "id index survives reopen");
    }

    if (temporary) {
        std::error_code ec;
        std::filesystem::remove(dbPath, ec);
        std::filesystem::remove(dbPath.string() + "-wal", ec);
        std::filesystem::remove(dbPath.string() + "-shm", ec);
    }

    std::printf("RESULT %s\n", gFailures == 0 ? "PASS" : "FAIL");
    return gFailures == 0 ? 0 : 1;
}
