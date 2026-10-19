/**
 * test_mmr.cpp
 *
 * Lightweight correctness test for the MMR peak/root cache.
 * Exercises addEntry, getPeaks, getRoot, getProof, checkProof,
 * finalizeSet, setFinalized(false), updateEntry, and serialization
 * round-trip, verifying root hashes stay identical before/after caching.
 *
 * Build: make -C build test_mmr
 * Run:   ./build/test_mmr
 */

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_entry.hpp"
#include "org/minima/objects/mmr/m_m_r_entry_number.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniString;
using org::minima::objects::mmr::MMR;
using org::minima::objects::mmr::MMRData;
using org::minima::objects::mmr::MMREntry;
using org::minima::objects::mmr::MMREntryNumber;
using org::minima::objects::mmr::MMRProof;

static std::unique_ptr<MMRData> leafData(const std::string& text, const MiniNumber& sum) {
    MiniString ms(text);
    return MMRData::CreateMMRDataLeafNode(ms, sum);
}

static std::string rootHex(const MMR& mmr) {
    auto root = mmr.getRoot();
    if (!root) {
        return "";
    }
    return root->getData().to0xString();
}

static int mainAddRootProof() {
    MMR mmr;
    std::vector<std::string> roots;

    for (int i = 0; i < 10; ++i) {
        auto data = leafData("leaf" + std::to_string(i), MiniNumber(static_cast<long long>(i)));
        MMREntry entry = mmr.addEntry(*data);

        // Peaks and root must be available after each add.
        auto peaks = mmr.getPeaks();
        assert(!peaks.empty() && "getPeaks must not be empty after add");
        std::string r = rootHex(mmr);
        assert(!r.empty() && "getRoot must not be empty after add");
        roots.push_back(r);

        // Proof for the newly added entry must verify.
        MMRProof proof = mmr.getProof(entry.getEntryNumber());
        assert(mmr.checkProof(*data, proof) && "fresh proof must verify");
    }

    // Check that distinct operations give consistent root.
    std::string finalRoot = rootHex(mmr);
    assert(finalRoot == roots.back() && "final root must match last cached root");

    std::cout << "OK: add/root/proof cycle (10 leaves)" << std::endl;
    return 0;
}

static int mainFinalizeUnfinalize() {
    MMR mmr;
    std::vector<std::unique_ptr<MMRData>> leaves;
    for (int i = 0; i < 5; ++i) {
        auto data = leafData("f" + std::to_string(i), MiniNumber(static_cast<long long>(i * 10)));
        mmr.addEntry(*data);
        leaves.emplace_back(std::move(data));
    }

    std::string pre = rootHex(mmr);

    mmr.finalizeSet();
    assert(mmr.isFinalized() && "MMR must be finalized");
    std::string post = rootHex(mmr);
    assert(pre == post && "root must not change across finalize");

    auto peaks = mmr.getPeaks();
    assert(!peaks.empty() && "finalized peaks must be available");

    // Unfinalize and add more entries; root must change.
    mmr.setFinalized(false);
    assert(!mmr.isFinalized() && "MMR must be unfinalized");
    auto more = leafData("more", MiniNumber::ONE());
    mmr.addEntry(*more);
    std::string after = rootHex(mmr);
    assert(after != post && "root must change after adding to unfinalized MMR");

    std::cout << "OK: finalize/unfinalize" << std::endl;
    return 0;
}

static int mainUpdateEntry() {
    MMR mmr;
    std::vector<MMREntryNumber> entries;
    std::vector<std::unique_ptr<MMRData>> originals;
    for (int i = 0; i < 4; ++i) {
        auto data = leafData("u" + std::to_string(i), MiniNumber(static_cast<long long>(i + 1)));
        MMREntry entry = mmr.addEntry(*data);
        entries.push_back(entry.getEntryNumber());
        originals.emplace_back(std::move(data));
    }

    std::string preRoot = rootHex(mmr);

    // Update the first entry to a zero-value spent coin.
    MMREntryNumber target = entries[0];
    MMRProof oldProof = mmr.getProofToPeak(target);
    auto newData = leafData("updated", MiniNumber::ZERO());
    mmr.updateEntry(target, oldProof, *newData);

    std::string postRoot = rootHex(mmr);
    assert(preRoot != postRoot && "root must change after updateEntry");

    // The new data must verify with a fresh proof.
    MMRProof newProof = mmr.getProof(target);
    assert(mmr.checkProof(*newData, newProof) && "updated data must verify");

    std::cout << "OK: updateEntry" << std::endl;
    return 0;
}

static int mainSerializationRoundTrip() {
    MMR mmr;
    for (int i = 0; i < 7; ++i) {
        auto data = leafData("s" + std::to_string(i), MiniNumber(static_cast<long long>(i * 3)));
        mmr.addEntry(*data);
    }
    std::string pre = rootHex(mmr);

    std::ostringstream oss(std::ios::binary);
    mmr.writeDataStream(oss);

    std::istringstream iss(oss.str(), std::ios::binary);
    auto mmr2 = MMR::ReadFromStream(iss);

    std::string post = rootHex(*mmr2);
    assert(pre == post && "root must survive serialization round-trip");
    assert(mmr2->isFinalized() && "loaded MMR must be finalized");

    std::cout << "OK: serialization round-trip" << std::endl;
    return 0;
}

int main() {
    try {
        mainAddRootProof();
        mainFinalizeUnfinalize();
        mainUpdateEntry();
        mainSerializationRoundTrip();
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "=== All MMR cache tests passed ===" << std::endl;
    return 0;
}
