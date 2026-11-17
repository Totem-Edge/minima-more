// Offline cascade rebuild.
//
// The truncating resync left every saved cascade as a ~3.3k-node partial chain
// that starts at 2,415,815. Older level-9/10 super-parents are absent, so
// checkCascadeCorrect() fails once a new tip references one. This tool walks
// the full archive from block 1 and rebuilds the condensed cascade with the
// same addToTip()/cascadeChain() logic the node uses, then writes cascade.db.
//
// Usage:
//   rebuild_cascade --data /srv/hawk --archive /srv/hawk/1.0/databases/archivesql/archive \
//                   --out /srv/hawk/1.0/databases/cascade.rebuilt.db [--to N]
//
// --to defaults to archive_last - 2560 so restoretree() still has a live tree
// window. Run with the node stopped (the archive SQLite file is locked while
// the node is running).

#include "org/minima/database/archive/archive_manager.hpp"
#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/database/cascade/cascade_node.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/system/params/general_params.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::string data = "/srv/hawk";
    std::string archive = "/srv/hawk/1.0/databases/archivesql/archive";
    std::string out = "/srv/hawk/1.0/databases/cascade.rebuilt.db";
    long long to = -1;
    long long link_from = -1;
    long long link_to = -1;
    long long dump = -1;
    bool makeempty = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--data" && i + 1 < argc) data = argv[++i];
        else if (a == "--archive" && i + 1 < argc) archive = argv[++i];
        else if (a == "--out" && i + 1 < argc) out = argv[++i];
        else if (a == "--to" && i + 1 < argc) to = std::stoll(argv[++i]);
        else if (a == "--linkcheck-from" && i + 1 < argc) link_from = std::stoll(argv[++i]);
        else if (a == "--linkcheck-to" && i + 1 < argc) link_to = std::stoll(argv[++i]);
        else if (a == "--dump" && i + 1 < argc) dump = std::stoll(argv[++i]);
        else if (a == "--makeempty") makeempty = true;
    }

    org::minima::system::params::GeneralParams::DATA_FOLDER = data;

    if (makeempty) {
        const std::filesystem::path dbdir =
            std::filesystem::path(data) / "1.0" / "databases";
        org::minima::database::cascade::Cascade emptyCascade;
        emptyCascade.saveDB(dbdir / "cascade.db");
        org::minima::database::txpowtree::TxPowTree emptyTree;
        emptyTree.saveDB(dbdir / "chaintree.db");
        std::cout << "wrote empty cascade.db and chaintree.db under "
                  << dbdir.string() << "\n";
        return 0;
    }

    org::minima::database::archive::ArchiveManager arch;
    arch.loadDB(archive);

    if (dump >= 0) {
        auto b = arch.loadBlockFromNumber(
            org::minima::objects::base::MiniNumber(dump));
        if (!b) {
            std::cerr << "dump: missing block " << dump << "\n";
            return 2;
        }
        auto& txp = b->getTxPoW();
        std::cout << "block " << dump << " id " << txp.getTxPoWID()
                  << " parent " << txp.getParentID().to0xString() << "\n";
        for (int i = 0; i < 12; ++i) {
            const auto sp = txp.getSuperParent(i);
            const auto found = arch.exists(sp.to0xString());
            std::cout << "  super[" << i << "] " << sp.to0xString()
                      << " -> " << found.toString() << "\n";
        }
        return 0;
    }


    // Hash-linkage check: every block's parentID must equal the previous
    // height's txpowid. Heights can be contiguous while mixing forks, which
    // is exactly what breaks cascade super-parent resolution.
    if (link_from > 0 && link_to >= link_from) {
        auto prev = arch.loadBlockFromNumber(
            org::minima::objects::base::MiniNumber(link_from));
        if (!prev) {
            std::cerr << "linkcheck: missing block " << link_from << "\n";
            return 2;
        }
        long long checked = 0;
        for (long long n = link_from + 1; n <= link_to; ++n) {
            auto cur = arch.loadBlockFromNumber(
                org::minima::objects::base::MiniNumber(n));
            if (!cur) {
                std::cerr << "linkcheck: missing block " << n << "\n";
                return 3;
            }
            if (!cur->getTxPoW().getParentID().isEqual(
                    prev->getTxPoW().getTxPoWIDData())) {
                std::cout << "LINK MISMATCH at " << n
                          << " parent=" << cur->getTxPoW().getParentID().to0xString()
                          << " prev_id=" << prev->getTxPoW().getTxPoWIDData().to0xString()
                          << "\n";
                return 4;
            }
            prev = std::move(cur);
            ++checked;
        }
        std::cout << "linkcheck " << link_from << ".." << link_to
                  << " OK (" << checked << " links)\n";
        return 0;
    }


    auto first = arch.loadFirstBlock();
    auto last = arch.loadLastBlock();
    if (!first || !last) {
        std::cerr << "archive is empty\n";
        return 2;
    }

    const long long first_bn = first->getTxPoW().getBlockNumber().getAsLong();
    const long long last_bn = last->getTxPoW().getBlockNumber().getAsLong();
    if (to < 0 || to > last_bn) {
        to = last_bn > 2560 ? last_bn - 2560 : last_bn;
    }
    if (to < first_bn) to = last_bn;

    std::cout << "archive " << first_bn << ".." << last_bn
              << " rebuilding cascade 1.." << to << "\n";

    org::minima::database::cascade::Cascade casc;
    long long count = 0;

    for (long long start = first_bn; start <= to; start += 256) {
        const long long end = std::min(to, start + 255);
        auto blocks = arch.loadBlockRange(
            org::minima::objects::base::MiniNumber(start - 1),
            org::minima::objects::base::MiniNumber(end + 1), false);
        if (blocks.empty()) {
            std::cerr << "archive gap at " << start << "\n";
            return 3;
        }
        for (auto& b : blocks) {
            const long long bn = b->getTxPoW().getBlockNumber().getAsLong();
            if (bn < first_bn || bn > to) continue;
            casc.addToTip(b->getTxPoW());
            ++count;
            if ((count % 10000) == 0) {
                casc.cascadeChain();
                std::cout << "  " << count << " blocks, tip "
                          << casc.getTip()->getTxPoW().getBlockNumber().toString()
                          << " len " << casc.getLength() << "\n";
            }
        }
    }

    casc.cascadeChain();
    if (!casc.getTip()) {
        std::cerr << "no cascade tip\n";
        return 4;
    }

    const bool valid =
        org::minima::database::cascade::Cascade::checkCascadeCorrect(casc);
    std::cout << "blocks=" << count
              << " tip=" << casc.getTip()->getTxPoW().getBlockNumber().toString()
              << " length=" << casc.getLength()
              << " valid=" << (valid ? "true" : "false") << "\n";

    if (!valid) {
        std::cerr << "rebuilt cascade failed checkCascadeCorrect\n";
        return 5;
    }

    casc.saveDB(out);
    std::cout << "saved " << out << "\n";
    return 0;
}
