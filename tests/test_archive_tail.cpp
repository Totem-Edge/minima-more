#include "org/minima/database/archive/archive_manager.hpp"
#include "org/minima/database/archive/raw_archive_input.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/i_b_d.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp"

#include <filesystem>

#include <sqlite3.h>

#include <cstdio>
#include <memory>
#include <string>

using org::minima::database::archive::ArchiveManager;
using org::minima::database::archive::RawArchiveInput;
using org::minima::objects::IBD;
using org::minima::objects::base::MiniNumber;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("usage: %s <archive-db> <archive.raw.dat>\n", argv[0]);
        return 2;
    }
    sqlite3_initialize();

    ArchiveManager arch;
    arch.loadDB(argv[1]);

    MiniNumber lastarch = MiniNumber::ZERO();
    if (auto last = arch.loadLastBlock()) {
        lastarch = last->getTxPoW().getBlockNumber();
    }
    const int before = arch.getSize();

    RawArchiveInput rawin{std::filesystem::path(argv[2])};
    rawin.connect();
    long long added = 0;
    while (true) {
        std::unique_ptr<IBD> ibd = rawin.getNextIBD();
        auto& blocks = ibd->getTxBlocks();
        if (blocks.empty()) break;
        for (auto& block : blocks) {
            if (block->getTxPoW().getBlockNumber().isMore(lastarch)) {
                arch.saveBlock(*block);
                ++added;
            }
        }
    }
    rawin.stop();
    arch.flushPending();

    auto last = arch.loadLastBlock();
    std::printf("before=%d added=%lld after=%d last=%s\n", before, added, arch.getSize(),
                last ? last->getTxPoW().getBlockNumber().toString().c_str() : "none");
    return 0;
}
