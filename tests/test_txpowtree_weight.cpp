// Regression test: TxPowTree::recalculateTree() must accumulate subtree weight
// from the deepest block upwards.
//
// The tree keeps a root at the oldest block and children at newer blocks. To
// sum weight bottom-up, recalculateTree() sorts nodes by *descending* block
// number and then adds each child's total into its parent. If the sort is
// ascending (parent first), a parent only ever sees its children's base
// weight, never their accumulated subtree, and fork choice can end up
// comparing two branches that both appear to carry the same work.
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/tx_po_w.hpp"

using org::minima::database::txpowtree::TxPoWTreeNode;
using org::minima::database::txpowtree::TxPowTree;
using org::minima::objects::TxPoW;
using org::minima::objects::base::MiniNumber;

namespace {

int g_failures = 0;

std::shared_ptr<TxPoWTreeNode> mkNode(const std::string& zID, int zBlock, int zWeight) {
    // Test constructor path: no DB, no MMR construction from a TxBlock.
    TxPoW txp(zID, zBlock, zWeight, /*zIsBlock=*/true, "0x00", /*zIsTransaction=*/false);
    return std::make_shared<TxPoWTreeNode>(txp);
}

void checkWeight(const char* zLabel, const std::shared_ptr<TxPoWTreeNode>& zNode, int zExpected) {
    const MiniNumber expected(zExpected);
    const MiniNumber& actual = zNode->getTotalWeight();
    if (actual.isEqual(expected)) {
        std::printf("  ok   %-14s total=%s\n", zLabel, actual.toString().c_str());
        return;
    }
    std::printf("  FAIL %-14s total=%s expected=%s\n",
                zLabel, actual.toString().c_str(), expected.toString().c_str());
    ++g_failures;
}

void checkTip(TxPowTree& zTree, int zExpectedBlock) {
    auto tip = zTree.getTip();
    if (!tip) {
        std::printf("  FAIL tip is null, expected block %d\n", zExpectedBlock);
        ++g_failures;
        return;
    }
    const MiniNumber expected(zExpectedBlock);
    const MiniNumber& actual = tip->getTxPoW().getBlockNumber();
    if (actual.isEqual(expected)) {
        std::printf("  ok   tip=%s\n", actual.toString().c_str());
        return;
    }
    std::printf("  FAIL tip=%s expected=%d\n", actual.toString().c_str(), zExpectedBlock);
    ++g_failures;
}

// Deep chain: verifies that grandchildren and great-grandchildren propagate.
void testDeepChainAccumulation() {
    std::printf("deep chain accumulation\n");
    auto n10  = mkNode("0x10", 10, 10);
    auto n11  = mkNode("0x11", 11, 10);
    auto n12a = mkNode("0x12a", 12, 5);
    auto n12b = mkNode("0x12b", 12, 5);
    auto n13a = mkNode("0x13a", 13, 100);
    auto n14a = mkNode("0x14a", 14, 100);

    n10->addChildNode(n11);
    n11->addChildNode(n12a);
    n11->addChildNode(n12b);
    n12a->addChildNode(n13a);
    n13a->addChildNode(n14a);

    TxPowTree tree;
    tree.setRoot(n10);

    checkWeight("H14a", n14a, 100);
    checkWeight("H13a", n13a, 200);   // 100 + 100
    checkWeight("H12a", n12a, 205);   // 5 + 200
    checkWeight("H12b", n12b, 5);
    checkWeight("H11", n11, 220);     // 10 + 205 + 5
    checkWeight("H10 root", n10, 230);
    checkTip(tree, 14);
}

// Fork: two equal-weight candidates at the fork height, but the second branch
// carries much more work deeper down. The tip must follow the heavier branch.
void testForkChoosesHeavierSubtree() {
    std::printf("fork chooses heavier subtree\n");
    auto n10  = mkNode("0x10", 10, 10);
    auto n11  = mkNode("0x11", 11, 10);
    auto n12a = mkNode("0x12a", 12, 5);   // light branch
    auto n13a = mkNode("0x13a", 13, 10);
    auto n12b = mkNode("0x12b", 12, 5);   // heavy branch, equal at the fork
    auto n13b = mkNode("0x13b", 13, 10);
    auto n14b = mkNode("0x14b", 14, 1000);

    n10->addChildNode(n11);
    n11->addChildNode(n12a);
    n11->addChildNode(n12b);
    n12a->addChildNode(n13a);
    n12b->addChildNode(n13b);
    n13b->addChildNode(n14b);

    TxPowTree tree;
    tree.setRoot(n10);

    checkWeight("H12a", n12a, 15);
    checkWeight("H12b", n12b, 1015);  // 5 + 10 + 1000
    checkWeight("H11", n11, 1040);    // 10 + 15 + 1015
    checkTip(tree, 14);
}

// setLength() feeds the cascade. Java returns only the new root's ancestors
// (the canonical blocks being aged out). A C++-only extension that also
// returned non-ancestor fork blocks below the new root corrupted the cascade
// because the caller links each entry onto the previous tip in hash-map order.
void testSetLengthReturnsMainChainOnly() {
    std::printf("setLength returns main-chain ancestors only\n");

    //        10 -> 11 -> 12a -> 13a -> 14a -> 15a   (heaviest branch)
    //               \-> 12b -> 13b                  (side branch below new root)
    auto n10  = mkNode("0x10", 10, 10);
    auto n11  = mkNode("0x11", 11, 10);
    auto n12a = mkNode("0x12a", 12, 10);
    auto n13a = mkNode("0x13a", 13, 10);
    auto n14a = mkNode("0x14a", 14, 10);
    auto n15a = mkNode("0x15a", 15, 10);
    auto n12b = mkNode("0x12b", 12, 10);
    auto n13b = mkNode("0x13b", 13, 10);

    n10->addChildNode(n11);
    n11->addChildNode(n12a);
    n12a->addChildNode(n13a);
    n13a->addChildNode(n14a);
    n14a->addChildNode(n15a);
    n11->addChildNode(n12b);
    n12b->addChildNode(n13b);

    TxPowTree tree;
    tree.setRoot(n10);

    // Tip is 15a, so setLength(3) keeps 13a/14a/15a and removes 10, 11, 12a.
    auto removed = tree.setLength(3);

    const std::vector<std::string> expected = {"0x10", "0x11", "0x12a"};
    bool ok = removed.size() == expected.size();
    std::printf("  %s removed size=%zu expected=%zu\n",
                ok ? "ok  " : "FAIL", removed.size(), expected.size());
    if (!ok) ++g_failures;

    for (size_t i = 0; i < removed.size() && i < expected.size(); ++i) {
        const std::string id = removed[i]->getTxPoW().getTxPoWID();
        if (id != expected[i]) {
            std::printf("  FAIL removed[%zu]=%s expected=%s\n",
                        i, id.c_str(), expected[i].c_str());
            ++g_failures;
        }
    }

    // The side branch must never be handed to the cascade.
    for (const auto& node : removed) {
        if (node->getTxPoW().getTxPoWID() == "0x12b") {
            std::printf("  FAIL non-ancestor 0x12b returned for cascade\n");
            ++g_failures;
        }
    }

    auto checkId = [](const char* label, const std::shared_ptr<TxPoWTreeNode>& node,
                      const char* want) {
        const std::string got = node ? node->getTxPoW().getTxPoWID() : "<null>";
        if (got == want) {
            std::printf("  ok   %s=%s\n", label, got.c_str());
        } else {
            std::printf("  FAIL %s=%s expected=%s\n", label, got.c_str(), want);
            ++g_failures;
        }
    };
    checkId("root", tree.getRoot(), "0x13a");
    checkId("tip", tree.getTip(), "0x15a");

    const int size = tree.getSize();
    if (size == 3) {
        std::printf("  ok   tree size=%d\n", size);
    } else {
        std::printf("  FAIL tree size=%d expected=3\n", size);
        ++g_failures;
    }
}

} // namespace

int main() {
    testDeepChainAccumulation();
    testForkChoosesHeavierSubtree();
    testSetLengthReturnsMainChainOnly();

    if (g_failures != 0) {
        std::printf("FAILED: %d check(s)\n", g_failures);
        return 1;
    }
    std::printf("PASS\n");
    return 0;
}
