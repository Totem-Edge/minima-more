// Level G — fork-choice and weight properties.
//
// These are randomized, seeded property tests over the production tree classes.
// They assert the invariants the protocol relies on, independent of any single
// golden vector:
//
//   P1  every node total == own block weight + sum(children totals)
//   P2  the root total == the sum of every block's base weight (conservation)
//   P3  the selected tip is the heaviest-child walk from the root (ties go to
//       the first child, matching Java)
//   P4  recalculation is idempotent
//   P5  setLength returns exactly the new root's ancestors, ascending, and no
//       non-ancestor fork block
//   P6  setLength leaves the tip unchanged and prunes every non-descendant
#include <cstdio>
#include <memory>
#include <random>
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

std::shared_ptr<TxPoWTreeNode> mkNode(int id, int block, int weight) {
    TxPoW txp("0x" + std::to_string(id), block, weight,
              /*zIsBlock=*/true, "0x00", /*zIsTransaction=*/false);
    return std::make_shared<TxPoWTreeNode>(txp);
}

MiniNumber ownWeight(const std::shared_ptr<TxPoWTreeNode>& node) {
    return MiniNumber::fromWideString(node->getTxPoW().getWeight());
}

MiniNumber childrenWeight(const std::shared_ptr<TxPoWTreeNode>& node) {
    MiniNumber total(0);
    for (const auto& child : node->getChildren()) {
        total = total.add(child->getTotalWeight());
    }
    return total;
}

int countDescendants(const std::shared_ptr<TxPoWTreeNode>& node) {
    int count = 1;
    for (const auto& child : node->getChildren()) {
        count += countDescendants(child);
    }
    return count;
}

void fail(const std::string& what) {
    std::printf("  FAIL %s\n", what.c_str());
    ++g_failures;
}

// Random tree: node i attaches to a random earlier node, block = parent + 1.
struct RandomTree {
    std::shared_ptr<TxPoWTreeNode> root;
    std::vector<std::shared_ptr<TxPoWTreeNode>> nodes;
};

RandomTree buildRandomTree(std::mt19937& rng, int count, int maxWeight) {
    RandomTree tree;
    tree.nodes.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        int block = 1;
        int parent = -1;
        if (i > 0) {
            parent = static_cast<int>(rng() % static_cast<unsigned>(i));
            block = tree.nodes[parent]->getBlockNumber().getAsInt() + 1;
        }
        int weight = 1 + static_cast<int>(rng() % static_cast<unsigned>(maxWeight));
        auto node = mkNode(i, block, weight);
        if (parent >= 0) tree.nodes[parent]->addChildNode(node);
        tree.nodes.push_back(node);
    }
    tree.root = tree.nodes.front();
    return tree;
}

void checkConservation(const RandomTree& tree) {
    MiniNumber sum(0);
    for (const auto& node : tree.nodes) {
        sum = sum.add(ownWeight(node));
        const MiniNumber expected = ownWeight(node).add(childrenWeight(node));
        if (!node->getTotalWeight().isEqual(expected)) {
            fail("node total != own + children (P1) at block " +
                 node->getBlockNumber().toString());
            return;
        }
        if (node->getTotalWeight().compareTo(ownWeight(node)) < 0) {
            fail("node total < own weight at block " +
                 node->getBlockNumber().toString());
            return;
        }
    }
    if (!tree.root->getTotalWeight().isEqual(sum)) {
        fail("root total != sum of base weights (P2)");
    }
}

void checkTip(TxPowTree& tree) {
    auto tip = tree.getTip();
    if (!tip) { fail("null tip"); return; }

    auto current = tree.getRoot();
    while (current != nullptr) {
        const auto& children = current->getChildren();
        if (children.empty()) break;
        auto best = children.front();
        for (const auto& child : children) {
            if (child->getTotalWeight().compareTo(best->getTotalWeight()) > 0) {
                best = child;
            }
        }
        current = best;
    }
    if (current != tip) {
        fail("tip is not the heaviest-child walk (P3)");
    }
    if (!tip->getChildren().empty()) {
        fail("tip still has children");
    }
}

void checkIdempotent(TxPowTree& tree, const RandomTree& random) {
    std::vector<std::string> before;
    before.reserve(random.nodes.size());
    for (const auto& node : random.nodes) {
        before.push_back(node->getTotalWeight().toString());
    }
    tree.recalculateTree();
    for (size_t i = 0; i < random.nodes.size(); ++i) {
        if (random.nodes[i]->getTotalWeight().toString() != before[i]) {
            fail("recalculateTree is not idempotent (P4)");
            return;
        }
    }
}

void testWeightProperties() {
    std::printf("random trees: conservation and tip selection\n");
    std::mt19937 rng(0xC0FFEEu);
    for (int run = 0; run < 200; ++run) {
        const int count = 2 + static_cast<int>(rng() % 60);
        RandomTree random = buildRandomTree(rng, count, 1000);
        TxPowTree tree;
        tree.setRoot(random.root);
        checkConservation(random);
        checkTip(tree);
        checkIdempotent(tree, random);
        if (g_failures) {
            std::printf("  (run %d, %d nodes)\n", run, count);
            return;
        }
    }
}

// Main chain plus leaf side branches; side weights are small so the heaviest
// path stays on the chain and the expected new root is deterministic.
void testSetLength() {
    std::printf("setLength: ancestors only, tip preserved, pruning complete\n");
    std::mt19937 rng(0xBEEFu);
    for (int run = 0; run < 100; ++run) {
        const int length = 3 + static_cast<int>(rng() % 20);
        std::vector<std::shared_ptr<TxPoWTreeNode>> chain;
        int id = 0;
        for (int h = 1; h <= length; ++h) {
            auto node = mkNode(id++, h, 1000);
            if (!chain.empty()) chain.back()->addChildNode(node);
            chain.push_back(node);
        }
        // side leaves below the eventual new root
        for (int h = 1; h < length; ++h) {
            const int sides = static_cast<int>(rng() % 3);
            for (int s = 0; s < sides; ++s) {
                chain[h - 1]->addChildNode(mkNode(id++, h + 1, 1));
            }
        }

        TxPowTree tree;
        tree.setRoot(chain.front());
        const int k = 1 + static_cast<int>(rng() % static_cast<unsigned>(length));
        const int expectedSize = countDescendants(chain[static_cast<size_t>(length - k)]);

        // Capture the expected ancestors before setLength(): setRoot() clears
        // the new root's parent pointer.
        auto expectedRoot = chain[static_cast<size_t>(length - k)];
        std::vector<std::shared_ptr<TxPoWTreeNode>> expected;
        for (auto p = expectedRoot->getParent(); p != nullptr; p = p->getParent()) {
            expected.insert(expected.begin(), p);
        }

        auto removed = tree.setLength(k);

        if (tree.getRoot() != expectedRoot) {
            fail("setLength selected the wrong new root (P5)");
            return;
        }
        if (tree.getTip() != chain.back()) {
            fail("setLength changed the tip (P6)");
            return;
        }
        if (tree.getSize() != expectedSize) {
            fail("setLength pruned the wrong node set (P6)");
            return;
        }

        // Expected: ancestors of the new root, oldest -> newest.
        if (removed.size() != expected.size()) {
            fail("setLength returned " + std::to_string(removed.size()) +
                 " blocks, expected " + std::to_string(expected.size()) +
                 " (P5) len=" + std::to_string(length) +
                 " k=" + std::to_string(k) +
                 " tip=" + tree.getTip()->getBlockNumber().toString() +
                 " newroot=" + tree.getRoot()->getBlockNumber().toString());
            return;
        }
        for (size_t i = 0; i < expected.size(); ++i) {
            if (removed[i] != expected[i]) {
                fail("setLength order/identity mismatch (P5)");
                return;
            }
        }
    }
}

} // namespace

int main() {
    testWeightProperties();
    testSetLength();
    if (g_failures) {
        std::printf("FAILED: %d check(s)\n", g_failures);
        return 1;
    }
    std::printf("PASS\n");
    return 0;
}
