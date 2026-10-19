/**
 * test_mempool_index.cpp
 *
 * Focused equivalence test for the RamDB mempool coin index (mCoinToTxPoW).
 * It builds a tiny transaction TxPoW carrying two coin proofs and asserts that
 * RamDB::checkForCoinID (now an O(1) hash lookup) returns the same answer as the
 * previous linear mempool scan across every mutation point:
 *   add / remove / setInCascade / cleanDB / wipeRamDB.
 *
 * A regression here means the index is not being maintained in sync with
 * mTxPoWDB, which would make the wallet/validation "is this coin in the
 * mempool?" test return wrong answers.
 */

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "org/minima/database/txpowdb/ram/ram_d_b.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"

using org::minima::database::txpowdb::ram::RamDB;
using org::minima::objects::Coin;
using org::minima::objects::CoinProof;
using org::minima::objects::Token;
using org::minima::objects::Transaction;
using org::minima::objects::TxPoW;
using org::minima::objects::Witness;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::mmr::MMRProof;

static int gPassed = 0;
static int gFailed = 0;

static void check(const std::string& zName, bool zGot, bool zExpected) {
    const bool ok = (zGot == zExpected);
    std::cout << "  " << zName << ": " << (ok ? "PASS" : "FAIL") << std::endl;
    if (!ok) {
        std::cout << "    got=" << (zGot ? "true" : "false")
                  << " exp=" << (zExpected ? "true" : "false") << std::endl;
    }
    ok ? gPassed++ : gFailed++;
}

// Build a tiny transaction TxPoW with `zCoins` as its witness inputs. The
// transaction body gets one input (so isTransaction() is true).
static std::shared_ptr<TxPoW> makeTxPoW(const std::vector<std::shared_ptr<Coin>>& zCoins) {
    auto txp = std::make_shared<TxPoW>();

    Transaction tran;
    if (!zCoins.empty()) {
        tran.addInput(zCoins.front());
    }
    txp->setTransaction(tran);

    Witness wit;
    for (const auto& c : zCoins) {
        wit.addCoinProof(std::make_unique<CoinProof>(c, std::make_shared<MMRProof>()));
    }
    txp->setWitness(wit);

    txp->calculateTXPOWID();
    return txp;
}

int main() {
    std::cout << "=== RamDB mempool coin index ===" << std::endl;

    const MiniData addr(
        "0x0000000000000000000000000000000000000000000000000000000000000000");
    const MiniData addr2(
        "0x1111111111111111111111111111111111111111111111111111111111111111");

    // Distinct input coins. The 3-arg Coin ctor sets coinID = 0x00, so use the
    // explicit-coinID ctor to get distinct IDs.
    const MiniData id1("0x1111111111111111111111111111111111111111111111111111111111111101");
    const MiniData id2("0x1111111111111111111111111111111111111111111111111111111111111102");
    const MiniData id3("0x2222222222222222222222222222222222222222222222222222222222222203");
    const MiniData id9("0x9999999999999999999999999999999999999999999999999999999999999909");

    auto c1 = std::make_shared<Coin>(id1, addr, MiniNumber(1), Token::TOKENID_MINIMA);
    auto c2 = std::make_shared<Coin>(id2, addr, MiniNumber(2), Token::TOKENID_MINIMA);
    check("distinct coin ids", !c1->getCoinID().isEqual(c2->getCoinID()), true);
    check("coin id round-trip", c1->getCoinID().isEqual(id1), true);

    RamDB ram;

    // Empty pool
    check("empty: id1 not found", ram.checkForCoinID(id1), false);
    check("empty: id2 not found", ram.checkForCoinID(id2), false);

    // add
    auto txp = makeTxPoW({c1, c2});
    const std::string txpid = txp->getTxPoWID();
    check("txp is a transaction", txp->isTransaction(), true);
    ram.addTxPoW(txp);
    check("after add: id1 found", ram.checkForCoinID(id1), true);
    check("after add: id2 found", ram.checkForCoinID(id2), true);
    check("after add: size == 1", ram.getSize() == 1, true);

    // remove
    ram.remove(txpid);
    check("after remove: id1 gone", ram.checkForCoinID(id1), false);
    check("after remove: id2 gone", ram.checkForCoinID(id2), false);
    check("after remove: size == 0", ram.getSize() == 0, true);

    // re-add then setInCascade -> excluded from the mempool index
    ram.addTxPoW(txp);
    check("re-add: id1 found", ram.checkForCoinID(id1), true);
    ram.setInCascade(txpid);
    check("after setInCascade: id1 excluded", ram.checkForCoinID(id1), false);
    check("after setInCascade: id2 excluded", ram.checkForCoinID(id2), false);
    check("after setInCascade: still stored", ram.getSize() == 1, true);

    // wipe
    ram.wipeRamDB();
    check("after wipe: id1 gone", ram.checkForCoinID(id1), false);
    check("after wipe: size == 0", ram.getSize() == 0, true);

    // cleanDB must preserve the index for a fresh, non-cascade tx
    auto c3 = std::make_shared<Coin>(id3, addr2, MiniNumber(3), Token::TOKENID_MINIMA);
    auto txp2 = makeTxPoW({c3});
    ram.addTxPoW(txp2);
    check("before cleanDB: id3 found", ram.checkForCoinID(id3), true);
    ram.cleanDB();
    check("after cleanDB: id3 still found", ram.checkForCoinID(id3), true);

    // a coin never added must stay absent
    auto c9 = std::make_shared<Coin>(id9, addr2, MiniNumber(999), Token::TOKENID_MINIMA);
    check("unknown coin absent", ram.checkForCoinID(c9->getCoinID()), false);

    std::cout << "\n=== " << gPassed << "/" << (gPassed + gFailed)
              << " mempool-index checks passed ===" << std::endl;
    return gFailed == 0 ? 0 : 1;
}
