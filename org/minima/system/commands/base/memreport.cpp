#include "org/minima/system/commands/base/memreport.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#if defined(__APPLE__)
#include <mach/mach.h>
#elif !defined(_WIN32)
#include <unistd.h>
#endif

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

namespace {

void readProcessMemory(long long& rss, long long& anon, long long& file) {
    rss = anon = file = 0;
#if defined(__linux__)
    {
        std::ifstream in("/proc/self/smaps_rollup");
        std::string key;
        long long val = 0;
        std::string unit;
        while (in >> key) {
            if (key == "Rss:" || key == "Anonymous:" || key == "Pss_File:") {
                in >> val >> unit;
                if (key == "Rss:") rss = val;
                else if (key == "Anonymous:") anon = val;
                else file = val;
            } else {
                std::string rest;
                std::getline(in, rest);
            }
        }
    }
    if (rss == 0) {
        std::ifstream in("/proc/self/status");
        std::string key;
        while (in >> key) {
            if (key == "VmRSS:") { in >> rss; break; }
            std::string rest;
            std::getline(in, rest);
        }
    }
#elif defined(__APPLE__)
    mach_task_basic_info_data_t info;
    mach_msg_type_number_t n = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &n) == KERN_SUCCESS) {
        rss = static_cast<long long>(info.resident_size / 1024);
        anon = rss;  // macOS: resident size (no cheap anon/file split here)
    }
#else
    (void)rss; (void)anon; (void)file;
#endif
}

// File size in kB, including the SQLite WAL companion (path + "-wal").
long long sizeKb(const std::filesystem::path& p) {
    std::error_code ec;
    auto sz = std::filesystem::file_size(p, ec);
    long long total = ec ? 0 : static_cast<long long>(sz);
    std::filesystem::path wal = p;
    wal += "-wal";
    auto wsz = std::filesystem::file_size(wal, ec);
    if (!ec) total += static_cast<long long>(wsz);
    return total / 1024;
}

} // namespace

memreport::memreport()
    : Command("memreport", "Live memory breakdown (process, databases, in-RAM structures)") {}

std::string memreport::getFullHelp() const {
    return std::string(
        "\nmemreport\n"
        "\n"
        "Returns a live memory breakdown: process RSS (total/anonymous/file), the\n"
        "on-disk size of each database (including -wal), and the sizes of the\n"
        "in-RAM structures (TxPoW tree, RAM TxPoW DB, cascade, tip MMR).\n"
        "\n"
        "memreport\n"
        "\n");
}

std::vector<std::string> memreport::getValidParams() const {
    return {};
}

CommandAnnotations memreport::getAnnotations() const {
    CommandAnnotations a;
    a.readOnly = true;
    a.destructive = false;
    a.idempotent = true;
    a.category = "base";
    return a;
}

std::unique_ptr<org::minima::utils::json::JSONObject> memreport::runCommand() {
    using org::minima::database::MinimaDB;
    using org::minima::utils::json::JSONObject;

    auto ret = getJSONReply();
    auto* db = MinimaDB::getDB();
    if (!db) {
        ret->put("error", std::string("database not available"));
        return ret;
    }

    long long rss = 0, anon = 0, file = 0;
    readProcessMemory(rss, anon, file);
    JSONObject proc;
    proc.put("rss_kb", rss);
    proc.put("anon_kb", anon);
    proc.put("file_kb", file);
    ret->put("process", proc);

    // Database files (kB, WAL included).
    JSONObject dbs;
    const char* names[] = {
        "chaintree.db", "cascade.db", "p2p.db", "p2p2.db", "userprefs.db",
        "walletsql/wallet", "txpowsql/txpow", "txpowsql/onchain",
        "archivesql/archive"};
    for (const char* n : names) {
        const long long kb = sizeKb(db->getDBFile(n));
        if (kb > 0) dbs.put(n, kb);
    }
    ret->put("databases_kb", dbs);

    // In-RAM structures.
    JSONObject comp;
    comp.put("txpow_tree_nodes", db->getTxPoWTree().getSize());
    comp.put("ram_txpow", db->getTxPoWDB().getRamSize());
    comp.put("cascade_length", db->getCascade().getLength());

    long long mmrEntries = 0;
    if (auto tip = db->getTxPoWTree().getTip()) {
        try { mmrEntries = tip->getMMR().getTotalEntries(); } catch (...) { mmrEntries = 0; }
    }
    comp.put("tip_mmr_entries", mmrEntries);
    comp.put("tip_mmr_est_kb", (mmrEntries * 635) / 1024);  // measured B/entry
    ret->put("components", comp);

    // SQLite cache budget in effect.
    JSONObject cache;
    cache.put("role_budget", std::string("4/8/16 MiB (p2p-userprefs / hot / archive)"));
    if (const char* e = std::getenv("MINIMA_SQLITE_CACHE_KIB")) cache.put("env_cache_kib", std::string(e));
    if (const char* e = std::getenv("MINIMA_SQLITE_MMAP_MB")) cache.put("env_mmap_mb", std::string(e));
    ret->put("sqlite", cache);

    ret->put("note", std::string(
        "database kB includes -wal; some state DBs persist on save/checkpoint; "
        "MMR estimate uses 635 B/entry"));
    return ret;
}

org::minima::system::commands::Command* memreport::getFunction() {
    return new memreport();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org
