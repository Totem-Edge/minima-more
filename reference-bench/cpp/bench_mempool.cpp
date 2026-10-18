// bench_mempool.cpp - Mempool (RamDB) RAM-scaling harness.
//
// The mempool is the other main in-memory growth structure besides the MMR: it
// holds unconfirmed transactions and their input coins. This harness measures
// process RSS vs the number of mempool transactions / input coins, plus build
// time. It also times checkForCoinID: this node keeps an O(1) coin->TxPoW index
// (mCoinToTxPoW), whereas the Java reference scans the whole pool linearly, so
// RAM-vs-lookup-speed is a real trade-off to quantify.
//
// Usage: bench_mempool [numTxns=100000] [coinsPerTx=2] [lookups=1000]
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

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <chrono>
#include <memory>
#include <vector>

#if defined(__APPLE__)
#include <mach/mach.h>
static long rssKb() {
    task_basic_info t; mach_msg_type_number_t c = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&t), &c) != KERN_SUCCESS) return -1;
    return static_cast<long>(t.resident_size / 1024);
}
#else
#include <fstream>
#include <unistd.h>
static long rssKb() {
    std::ifstream f("/proc/self/statm"); long size = 0, res = 0;
    if (!(f >> size >> res)) return -1;
    return res * (sysconf(_SC_PAGESIZE) / 1024);
}
#endif

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

// A deterministic, non-zero 32-byte coin id unique to (txIndex, coinIndex).
static MiniData coinId(std::uint32_t txIndex, std::uint32_t coinIndex) {
    std::vector<std::uint8_t> b(32, 0);
    b[0] = 0xAB;                                 // ensure non-zero
    b[4]  = static_cast<std::uint8_t>(txIndex);
    b[5]  = static_cast<std::uint8_t>(txIndex >> 8);
    b[6]  = static_cast<std::uint8_t>(txIndex >> 16);
    b[7]  = static_cast<std::uint8_t>(txIndex >> 24);
    b[8]  = static_cast<std::uint8_t>(coinIndex);
    b[9]  = static_cast<std::uint8_t>(coinIndex >> 8);
    return MiniData(b);
}

// Build a tiny transaction TxPoW whose witness carries the given input coins.
// setHeaderBodyHash() is required so different transactions hash to different
// TxPoW IDs (calculateTXPOWID hashes the header, which embeds the body hash).
static std::shared_ptr<TxPoW> makeTxPoW(const std::vector<std::shared_ptr<Coin>>& zCoins) {
    auto txp = std::make_shared<TxPoW>();
    Transaction tran;
    tran.addInput(zCoins.front());
    txp->setTransaction(tran);
    Witness wit;
    for (const auto& c : zCoins) {
        wit.addCoinProof(std::make_unique<CoinProof>(c, std::make_shared<MMRProof>()));
    }
    txp->setWitness(wit);
    txp->setHeaderBodyHash();
    txp->calculateTXPOWID();
    return txp;
}

int main(int argc, char** argv) {
    const int numTxns   = (argc > 1) ? std::atoi(argv[1]) : 100000;
    const int coinsPerTx = (argc > 2) ? std::atoi(argv[2]) : 2;
    const int lookups    = (argc > 3) ? std::atoi(argv[3]) : 1000;

    const MiniData addr("0x0000000000000000000000000000000000000000000000000000000000000000");

    const long base = rssKb();
    std::printf("rss_baseline_kb\t%ld\tkB\n", base);

    RamDB ram;
    std::vector<MiniData> present;
    present.reserve(static_cast<std::size_t>(lookups));

    const auto tb0 = std::chrono::steady_clock::now();
    for (int i = 0; i < numTxns; ++i) {
        std::vector<std::shared_ptr<Coin>> coins;
        coins.reserve(static_cast<std::size_t>(coinsPerTx));
        for (int j = 0; j < coinsPerTx; ++j) {
            coins.push_back(std::make_shared<Coin>(
                coinId(static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(j)),
                addr, MiniNumber(1 + i), Token::TOKENID_MINIMA));
        }
        auto txp = makeTxPoW(coins);
        ram.addTxPoW(txp);
        if (static_cast<int>(present.size()) < lookups) {
            present.push_back(coins.front()->getCoinID());
        }
    }
    const auto tb1 = std::chrono::steady_clock::now();

    const long after = rssKb();
    const double deltaBytes = static_cast<double>(after - base) * 1024.0;
    const double coins = static_cast<double>(numTxns) * coinsPerTx;

    std::printf("mempool_txns\t%d\t\n", ram.getSize());
    std::printf("mempool_coins\t%.0f\t\n", coins);
    std::printf("mempool_build_ms\t%.1f\tms\n",
                std::chrono::duration<double, std::milli>(tb1 - tb0).count());
    std::printf("rss_after_mempool_kb\t%ld\tkB\n", after);
    std::printf("mempool_delta_kb\t%ld\tkB\n", after - base);
    std::printf("mempool_bytes_per_txpow\t%.2f\tB\n", numTxns > 0 ? deltaBytes / numTxns : 0.0);
    std::printf("mempool_bytes_per_coin\t%.2f\tB\n", coins > 0 ? deltaBytes / coins : 0.0);

    // Lookup timing: how fast is "is this coin in the mempool?" (double-spend
    // check). C++ is O(1) per lookup via the coin index.
    long hits = 0;
    const auto tl0 = std::chrono::steady_clock::now();
    for (const auto& id : present) {
        if (ram.checkForCoinID(id)) ++hits;
    }
    const auto tl1 = std::chrono::steady_clock::now();
    const double lookupMs = std::chrono::duration<double, std::milli>(tl1 - tl0).count();
    std::printf("mempool_lookups\t%zu\t\n", present.size());
    std::printf("mempool_lookup_hits\t%ld\t\n", hits);
    std::printf("mempool_lookup_ms\t%.2f\tms\n", lookupMs);
    if (!present.empty()) {
        std::printf("mempool_lookup_us_per\t%.2f\tus\n", (lookupMs * 1000.0) / present.size());
    }
    return 0;
}
