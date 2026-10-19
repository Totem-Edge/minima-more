// RFC-0003 — engine benchmark runner.
//
// Runs the R1 (archive) and R2 (coin) workloads from RFC-0003 §3 against every
// engine that is compiled in, over one deterministic dataset, and emits a raw
// TSV plus ENGINE_BENCH_SUMMARY.md. Single-thread latency (p50/p95) and bulk
// throughput are reported separately; peak RSS and on-disk size per engine.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>  // sysconf/_SC_PAGESIZE for RSS on Linux
#endif

#include "dataset.hpp"
#include "interface.hpp"

namespace fs = std::filesystem;
using namespace eng;

// ---------------------------------------------------------------- utilities
static double now() {
    using C = std::chrono::steady_clock;
    return std::chrono::duration<double>(C::now().time_since_epoch()).count();
}

#ifdef __APPLE__
#include <mach/mach.h>
static double rssMB() {
    mach_task_basic_info_data_t info;
    mach_msg_type_number_t n = MACH_TASK_BASIC_INFO_COUNT;
    task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &n);
    return info.resident_size / 1048576.0;
}
#else
static double rssMB() {
    std::FILE* f = std::fopen("/proc/self/statm", "r");
    if (!f) return 0;
    long pages = 0, res = 0;
    if (std::fscanf(f, "%ld %ld", &pages, &res) != 2) res = 0;
    std::fclose(f);
    return res * (sysconf(_SC_PAGESIZE) / 1048576.0);
}
#endif

static void rmrf(const std::string& p) { std::error_code ec; fs::remove_all(p, ec); }

static double sizeMB(const std::string& p) {
    std::error_code ec;
    if (fs::is_regular_file(p, ec)) return (double)fs::file_size(p, ec) / 1048576.0;
    uintmax_t total = 0;
    for (auto& e : fs::recursive_directory_iterator(
             p, fs::directory_options::skip_permission_denied, ec))
        if (e.is_regular_file(ec)) total += e.file_size(ec);
    return total / 1048576.0;
}
// count a store's footprint incl. SQLite's sidecar WAL.
static double dirSizeMB(const std::string& p) { return sizeMB(p) + sizeMB(p + "-wal"); }

static double pct(std::vector<double>& v, double q) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    size_t i = std::min(v.size() - 1, (size_t)(q * v.size()));
    return v[i];
}
static std::string f2(double v) { char b[32]; std::snprintf(b, sizeof b, "%.2f", v); return b; }

struct Row { std::string store, metric, value, unit; };

// ---------------------------------------------------------------- R1 archive
static void runArchive(const std::string& eng, std::unique_ptr<ArchiveStore> s,
                       const Dataset& d, uint64_t K, uint64_t span,
                       const std::string& dir, std::vector<Row>& rows) {
    std::string path = dir + "/" + eng + ".archive";
    rmrf(path);
    OpenOpts o;

    double t0 = now();
    s->open(path, o);
    for (const auto& b : d.blocks) s->put(b);
    s->flush();
    double load = now() - t0;
    double peak = rssMB();
    rows.push_back({"archive", "bulk_load_rows_per_s",
                    std::to_string((uint64_t)(d.blocks.size() / load)), "rows/s"});

    // point get by height
    auto nums = sampleNums(d.blocks.size(), K, d.seed);
    std::vector<double> byNum;
    for (uint64_t n : nums) { Bytes out; double a = now(); s->getByNum(n, out);
                              byNum.push_back((now() - a) * 1e6); }
    auto sampleIds = sampleNums(d.blocks.size(), K, d.seed ^ 7);
    std::vector<double> byId;
    for (uint64_t i : sampleIds) { Bytes out; double a = now();
                                   s->getById(d.blocks[i].id, out); byId.push_back((now() - a) * 1e6); }

    // range scan (asc + desc)
    std::vector<uint64_t> got;
    double a = now(); s->scanRange(0, span, true, got); double scanAsc = (now() - a) * 1e3;
    got.clear();
    a = now(); s->scanRange(0, span, false, got); double scanDesc = (now() - a) * 1e3;

    uint64_t last = 0, first = 0, cnt = s->count();
    s->last(last); s->first(first);
    a = now(); uint64_t txns = 0, bytes = 0; s->analytics(txns, bytes);
    double anal = (now() - a) * 1e3;

    std::string snap = dir + "/" + eng + ".archive.snap";
    rmrf(snap);
    a = now(); s->snapshot(snap); double snapS = now() - a;
    double after = rssMB(); if (after > peak) peak = after;
    s->close();

    rows.push_back({"archive", "point_get_by_num_p50_us", f2(pct(byNum, 0.50)), "us"});
    rows.push_back({"archive", "point_get_by_num_p95_us", f2(pct(byNum, 0.95)), "us"});
    rows.push_back({"archive", "point_get_by_id_p50_us", f2(pct(byId, 0.50)), "us"});
    rows.push_back({"archive", "scan_asc_ms", std::to_string((int)scanAsc), "ms"});
    rows.push_back({"archive", "scan_desc_ms", std::to_string((int)scanDesc), "ms"});
    rows.push_back({"archive", "count_rows", std::to_string(cnt), ""});
    rows.push_back({"archive", "first_block", std::to_string(first), ""});
    rows.push_back({"archive", "last_block", std::to_string(last), ""});
    rows.push_back({"archive", "analytics_ms", std::to_string((int)anal), "ms"});
    rows.push_back({"archive", "analytics_txns", std::to_string(txns), ""});
    rows.push_back({"archive", "snapshot_s", std::to_string((int)snapS), "s"});
    rows.push_back({"archive", "peak_rss_mb", std::to_string((int)peak), "MB"});
    rows.push_back({"archive", "on_disk_mb", std::to_string((int)dirSizeMB(path)), "MB"});
    std::fprintf(stderr, "  [%s] archive done (%.1f MB)\n", eng.c_str(), dirSizeMB(path));
}

// ---------------------------------------------------------------- R2 coin
static void runCoin(const std::string& eng, std::unique_ptr<CoinStore> s,
                    const Dataset& d, uint64_t K, const std::string& dir,
                    std::vector<Row>& rows) {
    std::string path = dir + "/" + eng + ".coins";
    rmrf(path);
    OpenOpts o;

    double t0 = now();
    s->open(path, o);
    for (const auto& c : d.coins) s->put(c);
    s->flush();
    double load = now() - t0;
    double peak = rssMB();
    rows.push_back({"coin", "bulk_load_rows_per_s",
                    std::to_string((uint64_t)(d.coins.size() / load)), "rows/s"});

    auto ids = sampleCoinIds(d.coins, K, d.seed);
    std::vector<double> gets;
    for (const auto& id : ids) { Bytes out; double a = now(); s->get(id, out);
                                 gets.push_back((now() - a) * 1e6); }

    // prefix scan on a real 4-byte address prefix
    Bytes prefix(d.coins.empty() ? Bytes{} : Bytes(d.coins[0].address.begin(),
                                                   d.coins[0].address.begin() + 4));
    std::vector<Bytes> hits;
    double a = now(); s->prefixScan(prefix, hits); double pre = (now() - a) * 1e3;
    uint64_t cnt = s->count();

    std::string snap = dir + "/" + eng + ".coins.snap";
    rmrf(snap);
    a = now(); s->snapshot(snap); double snapS = now() - a;
    double after = rssMB(); if (after > peak) peak = after;
    s->close();

    rows.push_back({"coin", "point_get_p50_us", f2(pct(gets, 0.50)), "us"});
    rows.push_back({"coin", "point_get_p95_us", f2(pct(gets, 0.95)), "us"});
    rows.push_back({"coin", "prefix_scan_ms", std::to_string((int)pre), "ms"});
    rows.push_back({"coin", "prefix_scan_hits", std::to_string(hits.size()), ""});
    rows.push_back({"coin", "count", std::to_string(cnt), ""});
    rows.push_back({"coin", "snapshot_s", std::to_string((int)snapS), "s"});
    rows.push_back({"coin", "peak_rss_mb", std::to_string((int)peak), "MB"});
    rows.push_back({"coin", "on_disk_mb", std::to_string((int)dirSizeMB(path)), "MB"});
    std::fprintf(stderr, "  [%s] coin done (%.1f MB)\n", eng.c_str(), dirSizeMB(path));
}

// ---------------------------------------------------------------- output
int main(int argc, char** argv) {
    uint64_t blocks = 200000, coins = 120000, lookups = 2000, span = 1000;
    std::string out = ".";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() { return std::string(argv[++i]); };
        if (a == "--blocks") blocks = std::stoull(next());
        else if (a == "--coins") coins = std::stoull(next());
        else if (a == "--lookups") lookups = std::stoull(next());
        else if (a == "--scan-span") span = std::stoull(next());
        else if (a == "--out") out = next();
    }
    fs::create_directories(out);

    std::fprintf(stderr, "RSS start %.0f MB; generating dataset (%llu blocks, %llu coins)\n",
                 rssMB(), (unsigned long long)blocks, (unsigned long long)coins);
    Dataset d = makeDataset(blocks, coins, 0xC0FFEEULL);
    std::fprintf(stderr, "dataset ready; RSS %.0f MB\n", rssMB());

    struct Eng { const char* name; std::unique_ptr<ArchiveStore> (*ar)(); std::unique_ptr<CoinStore> (*co)(); };
    Eng list[] = {
        {"sqlite", make_sqlite_archive, make_sqlite_coin},
        {"sqlite-plain", make_sqlite_plain_archive, make_sqlite_plain_coin},
        {"lmdb", make_lmdb_archive, make_lmdb_coin},
        {"rocksdb", make_rocksdb_archive, make_rocksdb_coin},
        {"duckdb", make_duckdb_archive, make_duckdb_coin},
    };

    std::vector<Row> rows;
    std::vector<std::string> used;
    struct Pending { std::string name; std::vector<Row> rows; };
    std::vector<Pending> pending;

    for (auto& e : list) {
        auto ar = e.ar();
        auto co = e.co();
        if (!ar && !co) { std::fprintf(stderr, "engine %s: not built, skipped\n", e.name); continue; }
        std::fprintf(stderr, "== %s ==\n", e.name);
        Pending p;
        p.name = e.name;
        if (ar) runArchive(e.name, std::move(ar), d, lookups, span, out, p.rows);
        if (co) runCoin(e.name, std::move(co), d, lookups, out, p.rows);
        used.push_back(e.name);
        pending.push_back(std::move(p));
    }

    // flatten + tag each row with its engine for the summary
    for (auto& p : pending)
        for (auto& r : p.rows) rows.push_back(r);

    // summary (engine-tagged)
    std::ofstream md(out + "/ENGINE_BENCH_SUMMARY.md");
    md << "# RFC-0003 — embedded engine benchmark\n\n";
    md << "Host: " << (sizeof(void*) == 8 ? "64-bit" : "32-bit") << " · dataset: "
       << blocks << " blocks / " << coins << " coins · lookups: " << lookups
       << " · scan span: " << span << "\n\nEngines built: ";
    for (size_t i = 0; i < used.size(); ++i) md << (i ? ", " : "") << "`" << used[i] << "`";
    md << "\n";
    for (const char* store : {"archive", "coin"}) {
        std::vector<std::string> order;
        for (auto& p : pending)
            for (auto& r : p.rows)
                if (r.store == store &&
                    std::find(order.begin(), order.end(), r.metric) == order.end())
                    order.push_back(r.metric);
        md << "\n## " << store << "\n\n| metric | ";
        for (auto& e : used) md << e << " | ";
        md << "\n|---|";
        for (size_t i = 0; i < used.size(); ++i) md << "---|";
        md << "\n";
        for (auto& metric : order) {
            md << "| " << metric << " | ";
            for (auto& e : used) {
                std::string v = "-";
                for (auto& p : pending)
                    if (p.name == e)
                        for (auto& r : p.rows)
                            if (r.store == store && r.metric == metric)
                                v = r.value + (r.unit.empty() ? "" : " " + r.unit);
                md << v << " | ";
            }
            md << "\n";
        }
    }
    md << "\nRaw: `raw.tsv`. Methodology and fairness statement: `README.md`.\n";
    md.close();

    std::ofstream tsv(out + "/raw.tsv");
    for (auto& p : pending)
        for (auto& r : p.rows)
            tsv << p.name << "\t" << r.store << "\t" << r.metric << "\t" << r.value << "\t" << r.unit << "\n";
    tsv.close();

    std::printf("DONE -> %s/ENGINE_BENCH_SUMMARY.md\n", out.c_str());
    return 0;
}
