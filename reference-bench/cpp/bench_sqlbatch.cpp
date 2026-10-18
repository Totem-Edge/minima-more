// bench_sqlbatch.cpp - TxPoWSqlDB write throughput: batched vs one-transaction-per-row.
//
// TxPoWs arrive in bursts (sync / tree load / restore). Before batching, every
// insert was its own transaction (WAL write + potential sync); now consecutive
// inserts share one transaction, committing every BATCH_MAX_ROWS (512) rows.
// This harness inserts N distinct TxPoWs both ways and reports inserts/sec. The
// TxPoW objects are built once and reused so only the DB write path is timed.
//
// Usage: bench_sqlbatch [N=50000]
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

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <chrono>
#include <memory>
#include <vector>
#include <string>
#include <filesystem>
#include <unistd.h>

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

static std::shared_ptr<TxPoW> makeTxPoW(int zIndex) {
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

int main(int argc, char** argv) {
    const int N = (argc > 1) ? std::atoi(argv[1]) : 50000;

    std::vector<std::shared_ptr<TxPoW>> txps;
    txps.reserve(N);
    const auto tb0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) txps.push_back(makeTxPoW(i));
    const auto tb1 = std::chrono::steady_clock::now();
    std::printf("txpow_build_ms\t%.1f\tms\n",
                std::chrono::duration<double, std::milli>(tb1 - tb0).count());

    std::filesystem::path dir = std::filesystem::temp_directory_path() /
                                ("minima_sqlbench_" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    auto run = [&](bool zDisableBatch) -> double {
        const std::string file = (dir / (zDisableBatch ? "unbatched.db" : "batched.db")).string();
        TxPoWSqlDB db;
        db.loadDB(file);
        TxPoWSqlDB::DISABLE_BATCH = zDisableBatch;

        const auto t0 = std::chrono::steady_clock::now();
        for (const auto& txp : txps) db.addTxPoW(*txp, false);
        db.flushBatch();
        const auto t1 = std::chrono::steady_clock::now();

        std::printf("%s_rows\t%d\t\n", zDisableBatch ? "unbatched" : "batched", db.getSize());
        return std::chrono::duration<double, std::milli>(t1 - t0).count();
    };

    const double unbatchedMs = run(true);
    const double batchedMs = run(false);
    TxPoWSqlDB::DISABLE_BATCH = false;

    std::printf("unbatched_ms\t%.1f\tms\n", unbatchedMs);
    std::printf("batched_ms\t%.1f\tms\n", batchedMs);
    std::printf("unbatched_inserts_per_sec\t%.0f\t/s\n", unbatchedMs > 0 ? N * 1000.0 / unbatchedMs : 0.0);
    std::printf("batched_inserts_per_sec\t%.0f\t/s\n", batchedMs > 0 ? N * 1000.0 / batchedMs : 0.0);
    std::printf("speedup\t%.2f\tx\n", batchedMs > 0 ? unbatchedMs / batchedMs : 0.0);

    std::filesystem::remove_all(dir);
    return 0;
}
