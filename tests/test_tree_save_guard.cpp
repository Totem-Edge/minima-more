// A tree node that was never fully constructed (no TxBlock/MMR) must fail the
// save with an exception, not a null dereference. This is the shutdown-save
// crash that left cascade.db ahead of chaintree.db and made the next start
// report "Your Cascade is BROKEN".
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>

int main() {
    using org::minima::database::txpowtree::TxPoWTreeNode;

    TxPoWTreeNode node; // default-constructed: mTxBlock and mMMR are null
    std::ostringstream out(std::ios::binary);

    bool threw = false;
    try {
        node.writeDataStream(out);
    } catch (const std::runtime_error&) {
        threw = true;
    }

    if (!threw) {
        std::cerr << "FAIL: null tree node write did not throw\n";
        return 1;
    }
    std::cout << "tree save guard ok\n";
    return 0;
}
