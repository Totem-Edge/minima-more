// Decode a retained TxPoW transaction body from the node's txpowsql/txpow DB.
// The archive stores a block's txnlist as IDs only; this fixture proves the
// same serialized body the indexer reads can be decoded into its inputs.
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/tx_po_w.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: test_txpow_body <hex-file>\n";
        return 2;
    }
    std::ifstream in(argv[1]);
    std::string hex;
    if (!(in >> hex)) {
        std::cerr << "cannot read " << argv[1] << "\n";
        return 2;
    }
    if (hex.size() % 2 != 0) {
        std::cerr << "odd hex length\n";
        return 2;
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        bytes.push_back(static_cast<std::uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16)));
    }

    auto pow = org::minima::objects::TxPoW::convertMiniDataVersion(
        org::minima::objects::base::MiniData(bytes));
    if (!pow) {
        std::cerr << "TxPoW decode failed\n";
        return 1;
    }
    const auto& tx = pow->getTransaction();
    const std::size_t inputs = tx.getAllInputs().size();
    const std::size_t outputs = tx.getAllOutputs().size();
    std::cout << "txid " << tx.getTransactionID().to0xString()
              << " inputs " << inputs << " outputs " << outputs << "\n";
    if (inputs == 0) {
        std::cerr << "expected at least one input in the fixture\n";
        return 1;
    }
    return 0;
}
