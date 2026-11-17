// Restore manifest regression: the sidecar must round-trip, detect tampering,
// and the SHA-256 helper must match a known vector.
#include "org/minima/system/commands/backup/mmrsync/mega_m_m_r_backup.hpp"

#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/objects/i_b_d.hpp"
#include "org/minima/objects/mmr/mega_m_m_r.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_header.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using org::minima::objects::base::MiniNumber;
using org::minima::objects::mmr::MegaMMR;
using org::minima::system::commands::backup::mmrsync::MegaMMRBackup;
using org::minima::system::commands::backup::mmrsync::RestoreManifest;

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) ++failures;
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

} // namespace

int main() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "hawk_restore_manifest_test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);

    const fs::path payload = dir / "payload.bin";
    {
        std::ofstream out(payload, std::ios::binary | std::ios::trunc);
        out << "hello";
    }

    // Known SHA-256 of "hello".
    const std::string expected =
        "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824";
    check(MegaMMRBackup::fileSha256(payload) == expected, "file sha256 matches known vector");

    MegaMMR mmr;
    mmr.addBlock(makeBlock(1));
    mmr.addBlock(makeBlock(2));
    org::minima::objects::IBD ibd;
    MegaMMRBackup backup(&mmr, &ibd);

    const auto manifest = MegaMMRBackup::buildManifest(backup, payload);
    const fs::path manifestPath = dir / "payload.manifest";
    check(MegaMMRBackup::writeManifest(manifestPath, manifest), "manifest written");

    const auto loaded = MegaMMRBackup::readManifest(manifestPath);
    check(loaded.present, "manifest present");
    check(loaded.sha256 == expected, "manifest sha256 round-trips");
    check(loaded.coins == manifest.coins, "manifest coins round-trip");
    check(loaded.first_block == manifest.first_block, "manifest first_block round-trips");

    std::string err;
    check(MegaMMRBackup::verifyManifest(loaded, backup, payload, err),
          "manifest verifies against payload");

    auto tampered = loaded;
    tampered.sha256 = std::string(64, '0');
    check(!MegaMMRBackup::verifyManifest(tampered, backup, payload, err),
          "tampered sha256 is rejected");

    auto wrongTip = loaded;
    wrongTip.tip_block += 1;
    check(!MegaMMRBackup::verifyManifest(wrongTip, backup, payload, err),
          "wrong tip_block is rejected");

    fs::remove_all(dir, ec);
    if (failures != 0) {
        std::cerr << failures << " restore manifest check(s) failed\n";
        return 1;
    }
    std::cout << "restore manifest: all checks passed\n";
    return 0;
}
