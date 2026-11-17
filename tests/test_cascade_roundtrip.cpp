// Cascade save/load round-trip: a persisted cascade must keep the levels it
// was saved with. readDataStream() used to re-run cascadeChain(), which
// re-pruned an already-cascaded chain and made a valid in-memory cascade fail
// checkCascadeCorrect on the next start.
#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/database/cascade/cascade_node.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/tx_header.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/params/global_params.hpp"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

using org::minima::database::cascade::Cascade;
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

TxPoW makePow(int block, const std::string& parent) {
    TxPoW txpow(seedId(block), block, 100, true, "0x00", false);
    txpow.getTxHeader().mBlockNumber = MiniNumber(block);
    for (int i = 0; i < org::minima::system::params::GlobalParams::MINIMA_CASCADE_LEVELS; ++i) {
        txpow.setSuperParent(i, MiniData::ZERO_TXPOWID());
    }
    if (!parent.empty()) {
        txpow.setSuperParent(0, MiniData(parent));
    }
    return txpow;
}

} // namespace

int main() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "hawk_cascade_roundtrip_test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    org::minima::system::params::GeneralParams::DATA_FOLDER = dir.string();

    Cascade casc;
    casc.addToTip(makePow(1, ""));
    casc.addToTip(makePow(2, seedId(1)));
    casc.addToTip(makePow(3, seedId(2)));

    auto tip = casc.getTip();
    auto mid = casc.getParent(tip);
    auto root = casc.getParent(mid);
    check(tip && mid && root, "three-node cascade built");
    if (!tip || !mid || !root) return 1;

    // Deliberately non-trivial levels: cascadeChain() would flatten these.
    tip->setLevel(0);
    mid->setLevel(0);
    root->setLevel(1);

    const fs::path file = dir / "cascade.db";
    casc.saveDB(file);

    Cascade loaded;
    loaded.loadDB(file);

    check(loaded.getLength() == 3, "length preserved");
    check(loaded.getTip() && loaded.getTip()->getLevel() == 0, "tip level preserved");
    auto lmid = loaded.getParent(loaded.getTip());
    check(lmid && lmid->getLevel() == 0, "middle level preserved");
    auto lroot = lmid ? loaded.getParent(lmid) : nullptr;
    check(lroot && lroot->getLevel() == 1,
          "root level preserved (readDataStream must not re-prune)");

    fs::remove_all(dir, ec);
    if (failures != 0) {
        std::cerr << failures << " cascade round-trip check(s) failed\n";
        return 1;
    }
    std::cout << "cascade round-trip: all checks passed\n";
    return 0;
}
