#include "org/minima/system/commands/base/memreport.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/txpowdb/ram/ram_data.hpp"
#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/database/cascade/cascade_node.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#if defined(__APPLE__)
#include <mach/mach.h>
#elif !defined(_WIN32)
#include <unistd.h>
#include <dlfcn.h>
#endif

#if defined(__linux__) && defined(__GLIBC__)
#include <malloc.h>
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

// Wire size of a TxPoW (bytes). This undercounts C++ object overhead, so it is
// a lower bound for the in-RAM cost; useful for relative attribution.
long long txpowWireBytes(const org::minima::objects::TxPoW& t) {
    try {
        auto md = org::minima::objects::base::MiniData::getMiniDataVersion(
            const_cast<org::minima::objects::TxPoW&>(t));
        return md ? static_cast<long long>(md->getBytes().size()) : 0;
    } catch (...) {
        return 0;
    }
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
    return {"dump", "trim"};
}

std::vector<ParamSpec> memreport::getParamSpecs() const {
    ParamSpec d;
    d.name = "dump";
    d.type = "boolean";
    d.required = false;
    d.defaultValue = "false";
    d.description = "Trigger a jemalloc heap profile dump (needs jemalloc + prof:true)";
    ParamSpec t;
    t.name = "trim";
    t.type = "boolean";
    t.required = false;
    t.defaultValue = "false";
    t.description = "Return free glibc arena pages to the OS (malloc_trim) before measuring";
    return {d, t};
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

    // Return free arena pages first so the report reflects the trimmed state.
    const bool trim = getBooleanParam("trim", false);
#if defined(__linux__) && defined(__GLIBC__)
    if (trim) {
        malloc_trim(0);
        ret->put("trimmed", std::string("malloc_trim(0)"));
    }
#else
    if (trim) ret->put("trimmed", std::string("unsupported"));
#endif

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

    // Byte accounting (wire size; shared TxPoW objects are counted per
    // structure, so the sum is an upper bound).
    long long ramBytes = 0;
    for (const auto& kv : db->getTxPoWDB().getCompleteMemPool()) {
        if (kv.second) {
            if (auto tp = kv.second->getTxPoW()) ramBytes += txpowWireBytes(*tp);
        }
    }

    long long treeBytes = 0, treeNodesWalked = 0, treeMmrEntries = 0;
    {
        auto node = db->getTxPoWTree().getTip();
        const int maxNodes = db->getTxPoWTree().getSize() + 8;
        while (node && treeNodesWalked < maxNodes) {
            treeBytes += txpowWireBytes(node->getTxPoW());
            try { treeMmrEntries += node->getMMR().getTotalEntries(); } catch (...) {}
            ++treeNodesWalked;
            auto parent = node->getParent();
            if (!parent || parent == node) break;
            node = parent;
        }
    }

    long long cascBytes = 0, cascNodesWalked = 0;
    {
        auto node = db->getCascade().getTip();
        const int maxNodes = db->getCascade().getLength() + 8;
        while (node && cascNodesWalked < maxNodes) {
            cascBytes += txpowWireBytes(node->getTxPoW());
            ++cascNodesWalked;
            auto parent = db->getCascade().getParent(node);
            if (!parent || parent == node) break;
            node = parent;
        }
    }

    comp.put("ram_txpow_wire_kb", ramBytes / 1024);
    comp.put("tree_wire_kb", treeBytes / 1024);
    comp.put("tree_mmr_entries", treeMmrEntries);
    comp.put("tree_mmr_est_kb", (treeMmrEntries * 635) / 1024);
    comp.put("cascade_wire_kb", cascBytes / 1024);
    comp.put("attributed_est_mb",
             (ramBytes + treeBytes + treeMmrEntries * 635 + cascBytes) / (1024 * 1024));

#if defined(__linux__) && defined(__GLIBC__)
    {
        struct mallinfo2 mi = mallinfo2();
        comp.put("glibc_arena_kb", static_cast<long long>(mi.arena) / 1024);
        comp.put("glibc_inuse_kb", static_cast<long long>(mi.uordblks) / 1024);
        comp.put("glibc_free_kb", static_cast<long long>(mi.fordblks) / 1024);
        comp.put("glibc_mmap_kb", static_cast<long long>(mi.hblkhd) / 1024);
        comp.put("glibc_retained_kb",
                 (static_cast<long long>(mi.fordblks) + static_cast<long long>(mi.keepcost)) / 1024);
    }
#endif
    ret->put("components", comp);

    // SQLite cache budget in effect.
    JSONObject cache;
    cache.put("role_budget", std::string("4/8/16 MiB (p2p-userprefs / hot / archive)"));
    if (const char* e = std::getenv("MINIMA_SQLITE_CACHE_KIB")) cache.put("env_cache_kib", std::string(e));
    if (const char* e = std::getenv("MINIMA_SQLITE_MMAP_MB")) cache.put("env_mmap_mb", std::string(e));
    ret->put("sqlite", cache);

    // Optional: ask jemalloc (if preloaded and prof:true) to dump a heap profile.
    if (getBooleanParam("dump", false)) {
        const char* pfx = std::getenv("MINIMA_HEAPDUMP_PREFIX");
        const std::string prefix = pfx ? pfx : "/tmp/jeprof";
        bool ok = false;
#if defined(__linux__)
        using mallctl_t = int (*)(const char*, void*, std::size_t*, void*, std::size_t);
        auto fn = reinterpret_cast<mallctl_t>(dlsym(RTLD_DEFAULT, "mallctl"));
        if (fn) {
            const char* p = prefix.c_str();
            ok = (fn("prof.dump", nullptr, nullptr, const_cast<char**>(&p), sizeof(p)) == 0);
        }
#endif
        ret->put("heapdump", ok ? prefix : std::string("unavailable"));
    }

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
