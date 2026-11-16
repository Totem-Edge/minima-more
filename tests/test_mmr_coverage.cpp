/**
 * test_mmr_coverage.cpp
 *
 * Regression test for MegaMMR coverage tracking: a cursor at the tree root
 * must not be mistaken for a complete UTXO set. A partial MMR (one built from a
 * cascade sync that never replayed old blocks) reports first_block > 1 or a
 * coverage break, and isCoverageComplete() is false.
 */

#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/mmr/mega_m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_header.hpp"
#include "org/minima/objects/tx_po_w.hpp"

#include <cstdio>
#include <sstream>
#include <string>

using org::minima::objects::base::MiniNumber;
using org::minima::objects::mmr::MegaMMR;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) ++g_failures;
}

std::string txpowId(int block) {
    char buf[80];
    std::snprintf(buf, sizeof(buf), "0x%064X", static_cast<unsigned>(block));
    return std::string(buf);
}

org::minima::objects::TxBlock makeBlock(int block) {
    org::minima::objects::TxPoW txpow(txpowId(block), block, 100, true, "0x00", false);
    txpow.getTxHeader().mBlockNumber = MiniNumber(block);
    return org::minima::objects::TxBlock(txpow);
}

void addRange(MegaMMR& mmr, int from, int to) {
    for (int b = from; b <= to; ++b) {
        mmr.addBlock(makeBlock(b));
    }
}

} // namespace

int main() {
    // A fresh store has no coverage information at all.
    {
        MegaMMR mmr;
        check(mmr.firstBlock() == -1, "fresh MMR reports unknown first block");
        check(!mmr.coverageBroken(), "fresh MMR is not marked broken");
        check(!mmr.isCoverageComplete(), "fresh MMR is not complete");
        check(mmr.coinCount() == 0, "fresh MMR holds no coins");
    }

    // Contiguous history from genesis is complete.
    {
        MegaMMR mmr;
        addRange(mmr, 1, 5);
        check(mmr.firstBlock() == 1, "genesis-start MMR reports first_block 1");
        check(!mmr.coverageBroken(), "contiguous history is not broken");
        check(mmr.isCoverageComplete(), "contiguous history is complete");
    }

    // A cascade-style start at block 5 covers 5.. and is not complete.
    {
        MegaMMR mmr;
        addRange(mmr, 5, 8);
        check(mmr.firstBlock() == 5, "partial MMR reports first_block 5");
        check(!mmr.coverageBroken(), "contiguous partial history is not broken");
        check(!mmr.isCoverageComplete(), "partial MMR is not complete even at the tip");
    }

    // A gap is latched.
    {
        MegaMMR mmr;
        mmr.addBlock(makeBlock(5));
        mmr.addBlock(makeBlock(7));
        check(mmr.coverageBroken(), "non-contiguous add marks coverage broken");
        check(!mmr.isCoverageComplete(), "broken coverage is not complete");
    }

    // Version 2 round-trip preserves the watermark.
    {
        MegaMMR mmr;
        addRange(mmr, 1, 3);
        std::stringstream ss(std::ios::binary | std::ios::in | std::ios::out);
        mmr.writeDataStream(ss);

        MegaMMR reloaded;
        reloaded.readDataStream(ss);
        check(reloaded.firstBlock() == 1, "v2 round-trip preserves first_block");
        check(reloaded.isCoverageComplete(), "v2 round-trip preserves completeness");
        check(reloaded.coinCount() == mmr.coinCount(), "v2 round-trip preserves coin count");
    }

    // A legacy v1 stream has no watermark and must be treated as partial.
    {
        MegaMMR legacy;
        org::minima::objects::mmr::MMR empty;
        std::stringstream ss(std::ios::binary | std::ios::in | std::ios::out);
        MiniNumber::WriteToStream(ss, 1);
        empty.writeDataStream(ss);
        MiniNumber::WriteToStream(ss, 0);

        legacy.readDataStream(ss);
        check(legacy.firstBlock() == -1, "v1 file reports unknown first block");
        check(!legacy.isCoverageComplete(), "v1 file is not assumed complete");
    }

    // clear() resets coverage.
    {
        MegaMMR mmr;
        addRange(mmr, 1, 3);
        mmr.clear();
        check(mmr.firstBlock() == -1, "clear resets first_block");
        check(!mmr.isCoverageComplete(), "clear resets completeness");
    }

    if (g_failures != 0) {
        std::printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("mmr coverage: all checks passed\n");
    return 0;
}
