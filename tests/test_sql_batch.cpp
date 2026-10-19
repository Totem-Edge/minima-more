/**
 * test_sql_batch.cpp
 *
 * Correctness test for the batched writes in TxPoWSqlDB (auto BEGIN/COMMIT every
 * BATCH_MAX_ROWS rows). It inserts more rows than one batch, then verifies:
 *   - the row count is exactly N (no inserts lost at batch boundaries),
 *   - rows from the first, a middle and the last batch read back,
 *   - re-inserting an existing row is ignored (INSERT OR IGNORE still holds),
 *   - the data survives closeAndReopen() (batches are committed, not dropped).
 *
 * A regression here means a write-transaction boundary is losing or corrupting
 * TxPoWDB rows.
 */

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <unistd.h>

#include "org/minima/database/txpowdb/sql/tx_po_w_sql_d_b.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"

using org::minima::database::txpowdb::sql::TxPoWSqlDB;
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

static std::shared_ptr<TxPoW> makeTxPoW(int zIndex) {
    // Distinct input coin per TxPoW -> distinct body hash -> distinct TxPoW ID.
    std::vector<std::uint8_t> idb(32, 0);
    idb[0] = 0xCD;
    idb[1] = static_cast<std::uint8_t>(zIndex);
    idb[2] = static_cast<std::uint8_t>(zIndex >> 8);
    idb[3] = static_cast<std::uint8_t>(zIndex >> 16);
    idb[4] = static_cast<std::uint8_t>(zIndex >> 24);
    const MiniData addr("0x0000000000000000000000000000000000000000000000000000000000000000");
    auto coin = std::make_shared<Coin>(MiniData(idb), addr, MiniNumber(1 + zIndex), Token::TOKENID_MINIMA);

    auto txp = std::make_shared<TxPoW>();
    Transaction tran;
    tran.addInput(coin);
    txp->setTransaction(tran);
    Witness wit;
    wit.addCoinProof(std::make_unique<CoinProof>(coin, std::make_shared<MMRProof>()));
    txp->setWitness(wit);
    txp->setHeaderBodyHash();
    txp->calculateTXPOWID();
    return txp;
}

int main() {
    std::cout << "=== TxPoWSqlDB batch writes ===" << std::endl;

    const int N = 1500; // spans several BATCH_MAX_ROWS (512) boundaries
    std::filesystem::path dir = std::filesystem::temp_directory_path() /
                                ("minima_sqlbatch_" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const std::string dbfile = (dir / "txpow.db").string();

    std::vector<std::string> ids;
    ids.reserve(N);
    std::vector<std::shared_ptr<TxPoW>> txps;
    txps.reserve(N);

    {
        TxPoWSqlDB db;
        db.loadDB(dbfile);

        for (int i = 0; i < N; ++i) {
            auto txp = makeTxPoW(i);
            ids.push_back(txp->getTxPoWID());
            txps.push_back(txp);
            db.addTxPoW(*txp, false);
        }

        check("size == N after batched inserts", db.getSize() == N, true);
        check("first row present", db.exists(ids.front()), true);
        check("middle row present", db.exists(ids[N / 2]), true);
        check("last row present", db.exists(ids.back()), true);

        // Re-insert the SAME TxPoW (same ID): INSERT OR IGNORE must keep N.
        db.addTxPoW(*txps.front(), false);
        db.addTxPoW(*txps[N / 2], false);
        check("re-insert ignored (size still N)", db.getSize() == N, true);

        // Force an explicit flush (no-op if already flushed) then reopen.
        db.flushBatch();
        db.closeAndReopen();

        check("size == N after closeAndReopen", db.getSize() == N, true);
        auto first = db.getTxPoW(ids.front());
        auto last = db.getTxPoW(ids.back());
        check("first row reads back after reopen", first != nullptr, true);
        check("last row reads back after reopen", last != nullptr, true);
        check("last row round-trips ID", (last && last->getTxPoWID() == ids.back()), true);
    }

    std::filesystem::remove_all(dir);

    std::cout << "\n=== " << gPassed << "/" << (gPassed + gFailed)
              << " sql-batch checks passed ===" << std::endl;
    return gFailed == 0 ? 0 : 1;
}
