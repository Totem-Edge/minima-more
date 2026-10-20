#include "org/minima/database/archive/archive_manager.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <random>
#include <set>
#include <string>
#include <sys/resource.h>
#include <vector>

using org::minima::database::archive::ArchiveManager;
using org::minima::objects::TxBlock;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;

namespace {

std::string isoTime(long long zMillis) {
    std::time_t secs = static_cast<std::time_t>(zMillis / 1000);
    std::tm tm{};
    gmtime_r(&secs, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

long long blockNum(const TxBlock& zBlock) {
    return zBlock.getTxPoW().getBlockNumber().getAsLong();
}

long long blockTime(const TxBlock& zBlock) {
    return zBlock.getTxPoW().getTimeMilli().getAsLong();
}

int failures = 0;

void check(bool zOk, const std::string& zWhat) {
    if (!zOk) {
        ++failures;
        std::printf("FAIL %s\n", zWhat.c_str());
    }
}

void queryDb(const std::string& zPath, long long& zCount, long long& zMin, long long& zMax,
             long long& zDistinctIds, long long& zGaps, long long& zMissingChunks) {
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(zPath.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
        std::printf("FAIL open db for stats\n");
        ++failures;
        return;
    }
    auto scalar = [&](const char* sql) -> long long {
        sqlite3_stmt* st = nullptr;
        long long v = -1;
        if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK &&
            sqlite3_step(st) == SQLITE_ROW) {
            v = sqlite3_column_int64(st, 0);
        }
        if (st) sqlite3_finalize(st);
        return v;
    };
    zCount = scalar("SELECT COUNT(*) FROM syncblock;");
    zMin = scalar("SELECT MIN(block) FROM syncblock;");
    zMax = scalar("SELECT MAX(block) FROM syncblock;");
    zDistinctIds = scalar("SELECT COUNT(DISTINCT txpowid) FROM syncblock;");
    zGaps = scalar("SELECT COUNT(*) FROM (SELECT block, LAG(block) OVER (ORDER BY block) AS prev "
                   "FROM syncblock) WHERE prev IS NOT NULL AND block != prev + 1;");
    zMissingChunks = scalar("SELECT COUNT(*) FROM syncblock s LEFT JOIN blockchunks b "
                            "ON s.chunkid = b.id WHERE s.chunkid >= 0 AND b.id IS NULL;");
    sqlite3_close(db);
}

struct SampleResult {
    long long block = 0;
    long long time = 0;
    double lookupUs = 0;
    bool ok = false;
};

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: %s <archive-db> [expected-count]\n", argv[0]);
        return 2;
    }
    const std::string dbPath = argv[1];
    const long long expected = argc > 2 ? std::atoll(argv[2]) : -1;

    sqlite3_initialize();

    long long count = 0, minb = 0, maxb = 0, distinctIds = 0, gaps = 0, missing = 0;
    queryDb(dbPath, count, minb, maxb, distinctIds, gaps, missing);
    std::printf("db count=%lld min=%lld max=%lld distinct_ids=%lld gaps=%lld missing_chunks=%lld\n",
                count, minb, maxb, distinctIds, gaps, missing);
    check(count > 0, "non-empty archive");
    check(minb == 1, "archive starts at block 1");
    check(count == distinctIds, "block numbers and TxPoW ids are unique");
    check(gaps == 0, "no block-number gaps");
    check(missing == 0, "no missing chunks");
    if (expected > 0) {
        check(count == expected, "count matches raw archive header");
        check(maxb == expected, "last block matches raw archive header");
    }

    ArchiveManager arch;
    arch.loadDB(dbPath);
    check(arch.getSize() == count, "getSize matches SQL count");

    auto first = arch.loadFirstBlock();
    auto last = arch.loadLastBlock();
    check(first != nullptr && blockNum(*first) == 1, "loadFirstBlock is block 1");
    check(last != nullptr && blockNum(*last) == maxb, "loadLastBlock is the tip");
    if (first) {
        std::printf("first block=1 time=%lld (%s)\n", blockTime(*first), isoTime(blockTime(*first)).c_str());
    }
    if (last) {
        std::printf("last block=%lld time=%lld (%s)\n", blockNum(*last), blockTime(*last),
                    isoTime(blockTime(*last)).c_str());
    }

    const long long n = maxb;
    std::set<long long> picks = {
        1, 2, 3, 4, 5, 10, 16, 32, 64, 100, 128, 200, 255, 256, 257, 300, 511, 512, 513,
        768, 1000, 1024, 2048, 4096, 8192, 10000, 25000, 50000, 100000, 150000, 200000,
        250000, 300000, 400000, 500000, 600000, 750000, 900000, 1000000, 1100000, 1250000,
        1400000, 1500000, 1600000, 1750000, 1900000, 2000000, 2100000, 2200000, 2300000};
    std::mt19937_64 rng(0xC0FFEEULL);
    std::uniform_int_distribution<long long> dist(1, n);
    while (picks.size() < 256) {
        picks.insert(dist(rng));
    }
    for (long long b : {n - 3, n - 2, n - 1, n}) {
        if (b >= 1) picks.insert(b);
    }

    std::vector<SampleResult> results;
    results.reserve(picks.size());
    std::vector<long long> sortedTimes;

    for (long long b : picks) {
        if (b < 1 || b > n) continue;
        const auto t0 = std::chrono::steady_clock::now();
        auto blk = arch.loadBlockFromNumber(MiniNumber(b));
        const auto t1 = std::chrono::steady_clock::now();

        SampleResult sr;
        sr.block = b;
        sr.lookupUs = std::chrono::duration<double, std::micro>(t1 - t0).count();
        check(blk != nullptr, "load block " + std::to_string(b));
        if (!blk) {
            results.push_back(sr);
            continue;
        }
        sr.time = blockTime(*blk);
        check(blockNum(*blk) == b, "block number for " + std::to_string(b));

        const std::string id = blk->getTxPoW().getTxPoWID();
        check(!id.empty(), "non-empty TxPoW id at " + std::to_string(b));
        auto byId = arch.loadBlock(id);
        check(byId != nullptr && blockNum(*byId) == b, "id lookup at " + std::to_string(b));

        if (b > 1) {
            auto prev = arch.loadBlockFromNumber(MiniNumber(b - 1));
            check(prev != nullptr, "load parent " + std::to_string(b - 1));
            if (prev) {
                const std::string parent = blk->getTxPoW().getParentID().to0xString();
                const std::string prevId = prev->getTxPoW().getTxPoWIDData().to0xString();
                check(parent == prevId, "parent linkage at " + std::to_string(b));
                if (sr.time < blockTime(*prev)) {
                    std::printf("WARN timestamp out of order at %lld (delta %lld ms)\n",
                                b, sr.time - blockTime(*prev));
                }
            }
        }

        auto md = MiniData::getMiniDataVersion(*blk);
        check(md != nullptr, "serialize at " + std::to_string(b));
        if (md) {
            auto rt = TxBlock::convertMiniDataVersion(*md);
            check(rt != nullptr && rt->getTxPoW().getTxPoWID() == id,
                  "serialize round-trip at " + std::to_string(b));
        }

        sr.ok = true;
        results.push_back(sr);
        sortedTimes.push_back(sr.time);
    }

    std::vector<long long> starts = {1, 254, 510, 1000000 - 3, 1500000, 2000000, n - 4};
    for (long long s : starts) {
        if (s < 1) s = 1;
        if (s > n - 1) s = n - 1;
        auto range = arch.loadBlockRange(MiniNumber(s - 1), MiniNumber(std::min(n + 1, s + 4)), false);
        check(!range.empty(), "range load at " + std::to_string(s));
        long long expect = s;
        for (const auto& rb : range) {
            check(rb != nullptr && blockNum(*rb) == expect, "range sequence at " + std::to_string(expect));
            ++expect;
        }
    }

    std::sort(sortedTimes.begin(), sortedTimes.end());
    if (!sortedTimes.empty()) {
        check(sortedTimes.back() >= sortedTimes.front(), "timestamp span is valid");
        std::printf("sample times: min=%lld (%s) median=%lld (%s) max=%lld (%s)\n",
                    sortedTimes.front(), isoTime(sortedTimes.front()).c_str(),
                    sortedTimes[sortedTimes.size() / 2], isoTime(sortedTimes[sortedTimes.size() / 2]).c_str(),
                    sortedTimes.back(), isoTime(sortedTimes.back()).c_str());
    }

    std::vector<SampleResult> byBlock = results;
    std::sort(byBlock.begin(), byBlock.end(),
              [](const SampleResult& a, const SampleResult& b) { return a.block < b.block; });
    const std::size_t step = std::max<std::size_t>(1, byBlock.size() / 16);
    for (std::size_t i = 0; i < byBlock.size(); i += step) {
        const auto& r = byBlock[i];
        if (!r.ok) continue;
        std::printf("sample block=%lld time=%lld (%s) lookup_us=%.0f\n",
                    r.block, r.time, isoTime(r.time).c_str(), r.lookupUs);
    }

    double totalUs = 0;
    double maxUs = 0;
    int okCount = 0;
    for (const auto& r : results) {
        totalUs += r.lookupUs;
        maxUs = std::max(maxUs, r.lookupUs);
        if (r.ok) ++okCount;
    }
    std::printf("lookups=%zu ok=%d fail=%d avg_us=%.1f max_us=%.1f\n",
                results.size(), okCount, failures, results.empty() ? 0.0 : totalUs / results.size(), maxUs);

    struct rusage ru {};
    getrusage(RUSAGE_SELF, &ru);
    std::printf("maxrss=%ld %s\n", ru.ru_maxrss,
#ifdef __APPLE__
                "(bytes)"
#else
                "(KB)"
#endif
    );

    std::printf("RESULT %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
