#include "api_server.hpp"

#include "archive_reader.hpp"
#include "metric_catalog.hpp"

#include <duckdb.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace minima::analytics {

namespace {

std::atomic<bool> gStop{false};

void onSignal(int) {
    gStop.store(true);
}

std::string quote(const std::string& value) {
    std::string out = "\"";
    for (unsigned char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += "\"";
    return out;
}

std::string num(long long value) {
    return std::to_string(value);
}

std::string errorJson(const std::string& code, const std::string& message,
                      const std::string& requires, const std::string& available,
                      const std::string& remediation) {
    std::ostringstream out;
    out << "{\"error\":{\"code\":" << quote(code)
        << ",\"message\":" << quote(message)
        << ",\"requires\":" << requires
        << ",\"available_modes\":" << available
        << ",\"remediation\":" << quote(remediation) << "}}";
    return out.str();
}

struct IndexState {
    bool exists = false;
    bool ready = false;
    std::string status = "not_built";
    long long lastHeight = -1;
    long long lagBlocks = -1;
    std::string lastHash;
    bool mmrAvailable = false;
    long long mmrCoins = -1;
    long long mmrNative = -1;
    std::string mmrRoot;
    std::string mmrEngine;
};

IndexState readIndexState(const std::string& path) {
    IndexState out;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return out;
    out.exists = true;
    try {
        duckdb::DuckDB db(path);
        duckdb::Connection con(db);
        auto res = con.Query(
            "SELECT status, last_height, lag_blocks, last_hash "
            "FROM analytics.index_state WHERE id = 1;");
        if (res && !res->HasError() && res->RowCount() > 0) {
            out.status = res->GetValue(0, 0).ToString();
            if (!res->GetValue(1, 0).IsNull()) out.lastHeight = res->GetValue(1, 0).GetValue<std::int64_t>();
            if (!res->GetValue(2, 0).IsNull()) out.lagBlocks = res->GetValue(2, 0).GetValue<std::int64_t>();
            if (!res->GetValue(3, 0).IsNull()) out.lastHash = res->GetValue(3, 0).ToString();
            out.ready = (out.status == "ready");
        }
        auto mmr = con.Query(
            "SELECT root, coin_count, native_count, store_engine FROM analytics.mmr_snapshots "
            "ORDER BY loaded_at_ms DESC LIMIT 1;");
        if (mmr && !mmr->HasError() && mmr->RowCount() > 0) {
            out.mmrAvailable = true;
            if (!mmr->GetValue(0, 0).IsNull()) out.mmrRoot = mmr->GetValue(0, 0).ToString();
            out.mmrCoins = mmr->GetValue(1, 0).GetValue<std::int64_t>();
            out.mmrNative = mmr->GetValue(2, 0).GetValue<std::int64_t>();
            if (!mmr->GetValue(3, 0).IsNull()) out.mmrEngine = mmr->GetValue(3, 0).ToString();
        }
    } catch (...) {
        out.status = "degraded";
    }
    return out;
}

bool knownMetric(const std::string& name) {
    for (std::size_t i = 0; i < kMetricCatalogSize; ++i) {
        if (name == kMetricCatalog[i].name) return true;
    }
    return false;
}

bool validMetricName(const std::string& name) {
    if (name.empty()) return false;
    for (char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

std::string metricsJson(const std::string& indexPath, const std::string& metric) {
    duckdb::DuckDB db(indexPath);
    duckdb::Connection con(db);
    auto res = con.Query(
        "SELECT granularity, bucket_start_ms, height, value, value_raw, complete "
        "FROM analytics.metric_series WHERE metric = '" + metric +
        "' ORDER BY bucket_start_ms;");
    if (!res || res->HasError()) return {};
    const std::string granularity = (res->RowCount() > 0 && !res->GetValue(0, 0).IsNull())
        ? res->GetValue(0, 0).ToString() : "1d";
    std::ostringstream out;
    out << "{\"metric\":" << quote(metric) << ",\"granularity\":" << quote(granularity)
        << ",\"points\":[";
    for (std::size_t i = 0; i < res->RowCount(); ++i) {
        if (i) out << ",";
        out << "{\"t\":" << res->GetValue(1, i).GetValue<std::int64_t>();
        out << ",\"height\":";
        if (res->GetValue(2, i).IsNull()) out << "null";
        else out << res->GetValue(2, i).GetValue<std::int64_t>();
        out << ",\"value\":";
        if (res->GetValue(3, i).IsNull()) out << "null";
        else out << res->GetValue(3, i).GetValue<double>();
        out << ",\"value_raw\":";
        if (res->GetValue(4, i).IsNull()) out << "null";
        else out << res->GetValue(4, i).ToString();
        out << ",\"complete\":" << (res->GetValue(5, i).GetValue<bool>() ? "true" : "false");
        out << "}";
    }
    out << "]}";
    return out.str();
}

std::string sqlLit(const std::string& value) {
    std::string out = "'";
    for (char c : value) {
        if (c == '\'') out += "''";
        else out += c;
    }
    out += "'";
    return out;
}

std::string blockJson(const std::string& indexPath, long long height, bool& found) {
    found = false;
    duckdb::DuckDB db(indexPath);
    duckdb::Connection con(db);
    auto res = con.Query(
        "SELECT height,hash,prev_hash,timestamp_ms,tx_count,size_bytes,"
        "minter_address,reward_raw,fees_raw,finalized FROM analytics.chain_blocks "
        "WHERE height=" + std::to_string(height) + ";");
    if (!res || res->HasError() || res->RowCount() == 0) return {};
    found = true;
    std::ostringstream out;
    out << "{\"height\":" << res->GetValue(0,0).GetValue<std::int64_t>()
        << ",\"hash\":" << quote(res->GetValue(1,0).ToString())
        << ",\"prev_hash\":" << (res->GetValue(2,0).IsNull() ? "null" : quote(res->GetValue(2,0).ToString()))
        << ",\"timestamp_ms\":" << res->GetValue(3,0).GetValue<std::int64_t>()
        << ",\"tx_count\":" << res->GetValue(4,0).GetValue<std::int64_t>()
        << ",\"size_bytes\":";
    if (res->GetValue(5,0).IsNull()) out << "null"; else out << res->GetValue(5,0).GetValue<std::int64_t>();
    out << ",\"minter_address\":";
    if (res->GetValue(6,0).IsNull()) out << "null"; else out << quote(res->GetValue(6,0).ToString());
    out << ",\"finalized\":" << (res->GetValue(9,0).GetValue<bool>() ? "true" : "false") << "}";
    return out.str();
}

std::string txJson(const std::string& indexPath, const std::string& txid, bool& found) {
    found = false;
    duckdb::DuckDB db(indexPath);
    duckdb::Connection con(db);
    auto res = con.Query(
        "SELECT txpowid,height,tx_index,timestamp_ms,is_coinbase,fee_raw,"
        "input_count,output_count,size_bytes,burn_raw FROM analytics.txs WHERE txpowid=" +
        sqlLit(txid) + ";");
    if (!res || res->HasError() || res->RowCount() == 0) return {};
    found = true;
    std::ostringstream out;
    out << "{\"txpowid\":" << quote(res->GetValue(0,0).ToString())
        << ",\"height\":" << res->GetValue(1,0).GetValue<std::int64_t>()
        << ",\"timestamp_ms\":" << res->GetValue(3,0).GetValue<std::int64_t>()
        << ",\"is_coinbase\":" << (res->GetValue(4,0).GetValue<bool>() ? "true" : "false")
        << ",\"input_count\":" << res->GetValue(6,0).GetValue<std::int64_t>()
        << ",\"output_count\":" << res->GetValue(7,0).GetValue<std::int64_t>()
        << ",\"inputs\":[],\"outputs\":[]}";
    auto in = con.Query("SELECT input_index,coin_id,address,token_id,amount_raw,created_height "
                        "FROM analytics.tx_inputs WHERE txpowid=" + sqlLit(txid) +
                        " ORDER BY input_index LIMIT 1024;");
    auto on = con.Query("SELECT output_index,coin_id,address,token_id,amount_raw,created_height "
                        "FROM analytics.tx_outputs WHERE txpowid=" + sqlLit(txid) +
                        " ORDER BY output_index LIMIT 1024;");
    // Rebuild with populated arrays.
    std::ostringstream full;
    full << "{\"txpowid\":" << quote(res->GetValue(0,0).ToString())
         << ",\"height\":" << res->GetValue(1,0).GetValue<std::int64_t>()
         << ",\"timestamp_ms\":" << res->GetValue(3,0).GetValue<std::int64_t>()
         << ",\"input_count\":" << res->GetValue(6,0).GetValue<std::int64_t>()
         << ",\"output_count\":" << res->GetValue(7,0).GetValue<std::int64_t>()
         << ",\"inputs\":[";
    if (in && !in->HasError()) {
        for (std::size_t i = 0; i < in->RowCount(); ++i) {
            if (i) full << ",";
            full << "{\"index\":" << in->GetValue(0,i).GetValue<std::int64_t>()
                 << ",\"coin_id\":" << quote(in->GetValue(1,i).ToString())
                 << ",\"address\":" << (in->GetValue(2,i).IsNull() ? "null" : quote(in->GetValue(2,i).ToString()))
                 << ",\"amount_raw\":" << in->GetValue(4,i).ToString() << "}";
        }
    }
    full << "],\"outputs\":[";
    if (on && !on->HasError()) {
        for (std::size_t i = 0; i < on->RowCount(); ++i) {
            if (i) full << ",";
            full << "{\"index\":" << on->GetValue(0,i).GetValue<std::int64_t>()
                 << ",\"coin_id\":" << quote(on->GetValue(1,i).ToString())
                 << ",\"address\":" << (on->GetValue(2,i).IsNull() ? "null" : quote(on->GetValue(2,i).ToString()))
                 << ",\"amount_raw\":" << on->GetValue(4,i).ToString() << "}";
        }
    }
    full << "]}";
    return full.str();
}

std::string addressJson(const std::string& indexPath, const std::string& address,
                        const std::string& token, const std::string& day, bool& found) {
    found = false;
    duckdb::DuckDB db(indexPath);
    duckdb::Connection con(db);
    if (!token.empty()) {
        std::string dayClause;
        if (!day.empty()) {
            try { dayClause = " AND snapshot_day_ms=" + std::to_string(std::stoll(day)); }
            catch (...) { return {}; }
        }
        auto res = con.Query(
            "SELECT snapshot_day_ms,token_id,address,balance_raw,coin_count "
            "FROM analytics.asset_address_balances WHERE token_id=" + sqlLit(token) +
            " AND address=" + sqlLit(address) + dayClause +
            " ORDER BY snapshot_day_ms DESC LIMIT 1;");
        if (!res || res->HasError() || res->RowCount() == 0) return {};
        found = true;
        std::ostringstream out;
        out << "{\"address\":" << quote(res->GetValue(2,0).ToString())
            << ",\"token_id\":" << quote(res->GetValue(1,0).ToString())
            << ",\"balance_raw\":" << res->GetValue(3,0).ToString()
            << ",\"coin_count\":" << res->GetValue(4,0).ToString()
            << ",\"snapshot_day_ms\":" << res->GetValue(0,0).ToString() << "}";
        return out.str();
    }
    auto res = con.Query(
        "SELECT snapshot_height,address,balance_raw,coin_count FROM analytics.address_balances "
        "WHERE address=" + sqlLit(address) + " ORDER BY snapshot_height DESC LIMIT 1;");
    if (!res || res->HasError() || res->RowCount() == 0) return {};
    found = true;
    std::ostringstream out;
    out << "{\"address\":" << quote(res->GetValue(1,0).ToString())
        << ",\"balance_raw\":" << res->GetValue(2,0).ToString()
        << ",\"coin_count\":" << res->GetValue(3,0).ToString()
        << ",\"snapshot_height\":" << res->GetValue(0,0).ToString() << "}";
    return out.str();
}

std::string assetCohortsJson(const std::string& indexPath, const std::string& token,
                             const std::string& day, bool& found) {
    found = false;
    duckdb::DuckDB db(indexPath);
    duckdb::Connection con(db);
    std::string dayClause;
    if (!day.empty()) {
        try { dayClause = " AND snapshot_day_ms=" + std::to_string(std::stoll(day)); }
        catch (...) { return {}; }
    }
    auto res = con.Query(
        "SELECT snapshot_day_ms,token_id,cohort,address_count,balance_raw "
        "FROM analytics.asset_holder_cohorts WHERE token_id=" + sqlLit(token) + dayClause +
        " ORDER BY snapshot_day_ms DESC, cohort;");
    if (!res || res->HasError() || res->RowCount() == 0) return {};
    found = true;
    std::ostringstream out;
    out << "{\"token_id\":" << quote(token)
        << ",\"as_of_day_ms\":" << res->GetValue(0,0).ToString() << ",\"cohorts\":[";
    for (std::size_t i = 0; i < res->RowCount(); ++i) {
        if (i) out << ",";
        out << "{\"cohort\":" << quote(res->GetValue(2,i).ToString())
            << ",\"address_count\":" << res->GetValue(3,i).ToString()
            << ",\"balance_raw\":" << res->GetValue(4,i).ToString() << "}";
    }
    out << "]}";
    return out.str();
}

std::string utxoJson(const std::string& indexPath, bool& found) {
    found = false;
    duckdb::DuckDB db(indexPath);
    duckdb::Connection con(db);
    auto tip = con.Query("SELECT MAX(height) FROM analytics.utxo_snapshots;");
    if (!tip || tip->HasError() || tip->RowCount() == 0 || tip->GetValue(0,0).IsNull()) return {};
    const long long h = tip->GetValue(0,0).GetValue<std::int64_t>();
    found = true;
    auto snap = con.Query("SELECT coin_count,total_raw FROM analytics.utxo_snapshots WHERE height=" +
                          std::to_string(h) + ";");
    auto age = con.Query("SELECT band,coin_count,amount_raw FROM analytics.utxo_age_bands "
                         "WHERE snapshot_height=" + std::to_string(h) + " ORDER BY band;");
    auto val = con.Query("SELECT band,coin_count,amount_raw FROM analytics.utxo_value_bands "
                         "WHERE snapshot_height=" + std::to_string(h) + " ORDER BY band;");
    std::ostringstream out;
    out << "{\"as_of_height\":" << h << ",\"coin_count\":"
        << (snap && snap->RowCount() ? snap->GetValue(0,0).ToString() : "null")
        << ",\"total_raw\":" << (snap && snap->RowCount() ? snap->GetValue(1,0).ToString() : "null")
        << ",\"age_bands\":[";
    for (std::size_t i = 0; age && !age->HasError() && i < age->RowCount(); ++i) {
        if (i) out << ",";
        out << "{\"band\":" << quote(age->GetValue(0,i).ToString())
            << ",\"coin_count\":" << age->GetValue(1,i).ToString()
            << ",\"amount_raw\":" << age->GetValue(2,i).ToString() << "}";
    }
    out << "],\"value_bands\":[";
    for (std::size_t i = 0; val && !val->HasError() && i < val->RowCount(); ++i) {
        if (i) out << ",";
        out << "{\"band\":" << quote(val->GetValue(0,i).ToString())
            << ",\"coin_count\":" << val->GetValue(1,i).ToString()
            << ",\"amount_raw\":" << val->GetValue(2,i).ToString() << "}";
    }
    out << "]}";
    return out.str();
}

std::string cohortsJson(const std::string& indexPath, bool& found) {
    found = false;
    duckdb::DuckDB db(indexPath);
    duckdb::Connection con(db);
    auto res = con.Query("SELECT snapshot_height,cohort,address_count,balance_raw "
                         "FROM analytics.holder_cohorts WHERE snapshot_height="
                         "(SELECT MAX(snapshot_height) FROM analytics.holder_cohorts) ORDER BY cohort;");
    if (!res || res->HasError() || res->RowCount() == 0) return {};
    found = true;
    std::ostringstream out;
    out << "{\"as_of_height\":" << res->GetValue(0,0).GetValue<std::int64_t>()
        << ",\"cohorts\":[";
    for (std::size_t i = 0; i < res->RowCount(); ++i) {
        if (i) out << ",";
        out << "{\"cohort\":" << quote(res->GetValue(1,i).ToString())
            << ",\"address_count\":" << res->GetValue(2,i).ToString()
            << ",\"balance_raw\":" << res->GetValue(3,i).ToString() << "}";
    }
    out << "]}";
    return out.str();
}

std::string consistencyJson(const std::string& indexPath, bool& found) {
    found = false;
    duckdb::DuckDB db(indexPath);
    duckdb::Connection con(db);
    auto res = con.Query(
        "SELECT height,block_hash,archive_root,mmr_root,coin_count_archive,"
        "coin_count_mmr,matched,checked_at_ms,details FROM analytics.consistency_checks "
        "ORDER BY height DESC LIMIT 1;");
    if (!res || res->HasError() || res->RowCount() == 0) return {};
    found = true;
    std::ostringstream out;
    out << "{\"height\":" << res->GetValue(0,0).GetValue<std::int64_t>()
        << ",\"block_hash\":" << (res->GetValue(1,0).IsNull() ? "null" : quote(res->GetValue(1,0).ToString()))
        << ",\"mmr_root\":" << (res->GetValue(3,0).IsNull() ? "null" : quote(res->GetValue(3,0).ToString()))
        << ",\"archive_count\":" << res->GetValue(4,0).ToString()
        << ",\"mmr_count\":" << res->GetValue(5,0).ToString()
        << ",\"matched\":" << (res->GetValue(6,0).GetValue<bool>() ? "true" : "false")
        << ",\"checked_at_ms\":" << res->GetValue(7,0).GetValue<std::int64_t>()
        << ",\"details\":" << (res->GetValue(8,0).IsNull() ? "null" : quote(res->GetValue(8,0).ToString()))
        << ",\"comparable\":" << (res->GetValue(6,0).GetValue<bool>() ? "true" : "false") << "}";
    return out.str();
}

std::string queryParam(const std::string& query, const std::string& key) {
    std::size_t pos = 0;
    while (pos < query.size()) {
        const std::size_t end = query.find('&', pos);
        const std::string pair = query.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        const std::size_t eq = pair.find('=');
        const std::string k = eq == std::string::npos ? pair : pair.substr(0, eq);
        if (k == key) return eq == std::string::npos ? "" : pair.substr(eq + 1);
        if (end == std::string::npos) break;
        pos = end + 1;
    }
    return "";
}

std::string tokensJson(const std::string& indexPath, bool& found) {
    found = false;
    duckdb::DuckDB db(indexPath);
    duckdb::Connection con(db);
    auto res = con.Query(
        "SELECT snapshot_day_ms,snapshot_height,token_id,coin_count,total_raw,holder_count "
        "FROM analytics.asset_snapshots WHERE snapshot_day_ms="
        "(SELECT MAX(snapshot_day_ms) FROM analytics.asset_snapshots) ORDER BY total_raw DESC;");
    if (!res || res->HasError() || res->RowCount() == 0) return {};
    found = true;
    std::ostringstream out;
    out << "{\"as_of_day_ms\":" << res->GetValue(0,0).GetValue<std::int64_t>()
        << ",\"as_of_height\":" << res->GetValue(1,0).GetValue<std::int64_t>() << ",\"tokens\":[";
    for (std::size_t i = 0; i < res->RowCount(); ++i) {
        if (i) out << ",";
        out << "{\"token_id\":" << quote(res->GetValue(2,i).ToString())
            << ",\"coin_count\":" << res->GetValue(3,i).ToString()
            << ",\"total_raw\":" << res->GetValue(4,i).ToString()
            << ",\"holder_count\":" << res->GetValue(5,i).ToString() << "}";
    }
    out << "]}";
    return out.str();
}

std::string assetUtxoJson(const std::string& indexPath, const std::string& token,
                          const std::string& dayParam, bool& found) {
    found = false;
    duckdb::DuckDB db(indexPath);
    duckdb::Connection con(db);
    std::string dayClause;
    if (!dayParam.empty()) {
        try { dayClause = " AND snapshot_day_ms=" + std::to_string(std::stoll(dayParam)); }
        catch (...) { return {}; }
    }
    auto snap = con.Query(
        "SELECT snapshot_day_ms,snapshot_height,coin_count,total_raw,holder_count "
        "FROM analytics.asset_snapshots WHERE token_id=" + sqlLit(token) + dayClause +
        " ORDER BY snapshot_day_ms DESC LIMIT 1;");
    if (!snap || snap->HasError() || snap->RowCount() == 0) return {};
    found = true;
    const std::string day = std::to_string(snap->GetValue(0,0).GetValue<std::int64_t>());
    auto age = con.Query("SELECT band,coin_count,amount_raw FROM analytics.asset_age_bands "
                         "WHERE token_id=" + sqlLit(token) + " AND snapshot_day_ms=" + day +
                         " ORDER BY band;");
    auto val = con.Query("SELECT band,coin_count,amount_raw FROM analytics.asset_value_bands "
                         "WHERE token_id=" + sqlLit(token) + " AND snapshot_day_ms=" + day +
                         " ORDER BY band;");
    std::ostringstream out;
    out << "{\"token_id\":" << quote(token)
        << ",\"as_of_day_ms\":" << snap->GetValue(0,0).ToString()
        << ",\"as_of_height\":" << snap->GetValue(1,0).ToString()
        << ",\"coin_count\":" << snap->GetValue(2,0).ToString()
        << ",\"total_raw\":" << snap->GetValue(3,0).ToString()
        << ",\"holder_count\":" << snap->GetValue(4,0).ToString()
        << ",\"age_bands\":[";
    for (std::size_t i = 0; age && !age->HasError() && i < age->RowCount(); ++i) {
        if (i) out << ",";
        out << "{\"band\":" << quote(age->GetValue(0,i).ToString())
            << ",\"coin_count\":" << age->GetValue(1,i).ToString()
            << ",\"amount_raw\":" << age->GetValue(2,i).ToString() << "}";
    }
    out << "],\"value_bands\":[";
    for (std::size_t i = 0; val && !val->HasError() && i < val->RowCount(); ++i) {
        if (i) out << ",";
        out << "{\"band\":" << quote(val->GetValue(0,i).ToString())
            << ",\"coin_count\":" << val->GetValue(1,i).ToString()
            << ",\"amount_raw\":" << val->GetValue(2,i).ToString() << "}";
    }
    out << "]}";
    return out.str();
}

std::string snapshotsJson(const std::string& indexPath, bool& found) {
    found = false;
    duckdb::DuckDB db(indexPath);
    duckdb::Connection con(db);
    auto res = con.Query(
        "SELECT snapshot_day_ms,MAX(snapshot_height),COUNT(DISTINCT token_id),SUM(coin_count) "
        "FROM analytics.asset_snapshots GROUP BY snapshot_day_ms ORDER BY snapshot_day_ms DESC;");
    if (!res || res->HasError() || res->RowCount() == 0) return {};
    found = true;
    std::ostringstream out;
    out << "{\"snapshots\":[";
    for (std::size_t i = 0; i < res->RowCount(); ++i) {
        if (i) out << ",";
        out << "{\"day_ms\":" << res->GetValue(0,i).ToString()
            << ",\"height\":" << res->GetValue(1,i).ToString()
            << ",\"tokens\":" << res->GetValue(2,i).ToString()
            << ",\"coins\":" << res->GetValue(3,i).ToString() << "}";
    }
    out << "]}";
    return out.str();
}

std::string availableModesJson(const ApiOptions& opt) {
    std::error_code ec;
    const bool hasArchive = std::filesystem::exists(opt.archivePath, ec) || opt.forceArchive;
    const bool hasMmr = std::filesystem::exists(opt.mmrPath, ec) || opt.forceMmr;
    std::string out = "[";
    bool first = true;
    auto add = [&](const char* name) {
        if (!first) out += ",";
        out += quote(name);
        first = false;
    };
    if (hasArchive) add("archive");
    if (hasMmr) add("megammr");
    add("analytics");
    out += "]";
    return out;
}

std::string capabilitiesJson(const ApiOptions& opt) {
    ArchiveStats archive = readArchiveStats(opt.archivePath);
    const bool hasArchive = archive.exists || opt.forceArchive;

    std::error_code ec;
    const bool mmrExists = std::filesystem::exists(opt.mmrPath, ec);
    std::int64_t mmrSize = 0;
    if (mmrExists) {
        mmrSize = static_cast<std::int64_t>(std::filesystem::file_size(opt.mmrPath, ec));
    }
    const bool hasMmr = mmrExists || opt.forceMmr;

    std::ostringstream modes;
    modes << "[";
    bool first = true;
    auto addMode = [&](const char* name) {
        if (!first) modes << ",";
        modes << quote(name);
        first = false;
    };
    if (hasArchive) addMode("archive");
    if (hasMmr) addMode("megammr");
    addMode("analytics");
    modes << "]";

    std::vector<std::string> warnings;
    if (!hasArchive) warnings.push_back("archive_not_available");
    if (archive.exists && !archive.complete) warnings.push_back("archive_incomplete");
    if (archive.exists && !archive.error.empty()) warnings.push_back("archive_read_error");
    if (!hasMmr) warnings.push_back("megammr_not_available");

    std::ostringstream out;
    out << "{";
    out << "\"node\":{\"name\":\"minima-analytics\",\"version\":\"0.1.0\"},";
    out << "\"modes\":" << modes.str() << ",";
    out << "\"coverage\":{";

    out << "\"archive\":";
    if (archive.exists) {
        out << "{\"from\":" << num(archive.minBlock)
            << ",\"to\":" << num(archive.maxBlock)
            << ",\"count\":" << num(archive.count)
            << ",\"chunks\":" << num(archive.chunks)
            << ",\"missing_chunks\":" << num(archive.missingChunks)
            << ",\"complete\":" << (archive.complete ? "true" : "false")
            << ",\"tx_details\":true,\"tx_detail_from\":1"
            << ",\"size_bytes\":" << num(archive.sizeBytes) << "}";
    } else {
        out << "null";
    }

    out << ",\"megammr\":";
    const IndexState mmrState = readIndexState(opt.indexPath);
    if (mmrState.mmrAvailable || hasMmr) {
        out << "{\"as_of\":null,\"coins\":";
        if (mmrState.mmrAvailable) out << mmrState.mmrCoins; else out << "null";
        out << ",\"native_coins\":";
        if (mmrState.mmrAvailable) out << mmrState.mmrNative; else out << "null";
        out << ",\"root\":";
        if (mmrState.mmrAvailable && !mmrState.mmrRoot.empty()) out << quote(mmrState.mmrRoot);
        else out << "null";
        out << ",\"store_engine\":";
        if (mmrState.mmrAvailable && !mmrState.mmrEngine.empty()) out << quote(mmrState.mmrEngine);
        else out << "null";
        out << ",\"size_bytes\":" << num(mmrSize)
            << ",\"file\":" << quote(opt.mmrPath) << "}";
    } else {
        out << "null";
    }

    out << ",\"live\":null},";
    const IndexState idx = readIndexState(opt.indexPath);
    out << "\"index\":{\"status\":" << quote(idx.status)
        << ",\"schema\":1,\"lag_blocks\":";
    if (idx.lagBlocks < 0) out << "null"; else out << idx.lagBlocks;
    out << ",\"last_height\":";
    if (idx.lastHeight < 0) out << "null"; else out << idx.lastHeight;
    out << "},";
    out << "\"warnings\":[";
    for (std::size_t i = 0; i < warnings.size(); ++i) {
        if (i) out << ",";
        out << quote(warnings[i]);
    }
    out << "],";
    out << "\"limits\":{\"max_range_blocks\":100000,\"max_page\":1000,\"rate_limit_rps\":20}";
    out << "}";
    return out.str();
}

std::string catalogJson() {
    std::ostringstream out;
    out << "{\"metrics\":[";
    for (std::size_t i = 0; i < kMetricCatalogSize; ++i) {
        const MetricDef& m = kMetricCatalog[i];
        if (i) out << ",";
        out << "{\"name\":" << quote(m.name)
            << ",\"unit\":" << quote(m.unit)
            << ",\"granularity\":" << quote(m.granularity)
            << ",\"source\":" << quote(m.source)
            << ",\"min_mode\":" << quote(m.minMode)
            << ",\"description\":" << quote(m.description) << "}";
    }
    out << "]}";
    return out.str();
}

std::string coverageJson(const ApiOptions& opt) {
    const std::string caps = capabilitiesJson(opt);
    const std::size_t begin = caps.find("\"coverage\":");
    if (begin == std::string::npos) return "{}";
    const std::size_t body = caps.find('{', begin);
    int depth = 0;
    std::size_t end = body;
    for (; end < caps.size(); ++end) {
        if (caps[end] == '{') ++depth;
        if (caps[end] == '}') {
            --depth;
            if (depth == 0) break;
        }
    }
    if (body == std::string::npos || end >= caps.size()) return "{}";
    return caps.substr(body, end - body + 1);
}

void sendResponse(int fd, int status, const char* reason, const std::string& body) {
    std::ostringstream head;
    head << "HTTP/1.1 " << status << " " << reason << "\r\n"
         << "Content-Type: application/json\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Connection: close\r\n"
         << "Access-Control-Allow-Origin: *\r\n\r\n";
    const std::string out = head.str() + body;
    std::size_t sent = 0;
    while (sent < out.size()) {
        const ssize_t n = ::send(fd, out.data() + sent, out.size() - sent, 0);
        if (n <= 0) break;
        sent += static_cast<std::size_t>(n);
    }
}

void handleClient(int fd, const ApiOptions& opt) {
    char buffer[8192];
    const ssize_t n = ::recv(fd, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) return;
    buffer[n] = '\0';

    std::istringstream request(buffer);
    std::string method, target, version;
    request >> method >> target >> version;
    const std::size_t q = target.find('?');
    const std::string path = target.substr(0, q);
    const std::string query = q == std::string::npos ? "" : target.substr(q + 1);

    if (method != "GET") {
        sendResponse(fd, 405, "Method Not Allowed",
                     errorJson("method_not_allowed", "Only GET is supported", "[]", "[]",
                               "Use GET"));
        return;
    }

    if (path == "/v1/healthz") {
        sendResponse(fd, 200, "OK", "{\"status\":\"ok\"}");
        return;
    }
    if (path == "/v1/capabilities") {
        sendResponse(fd, 200, "OK", capabilitiesJson(opt));
        return;
    }
    if (path == "/v1/catalog") {
        sendResponse(fd, 200, "OK", catalogJson());
        return;
    }
    if (path == "/v1/coverage") {
        sendResponse(fd, 200, "OK", coverageJson(opt));
        return;
    }

    const std::string metricsPrefix = "/v1/metrics/";
    if (path.rfind(metricsPrefix, 0) == 0) {
        const std::string metric = path.substr(metricsPrefix.size());
        if (!validMetricName(metric)) {
            sendResponse(fd, 400, "Bad Request",
                         errorJson("invalid_metric", "Invalid metric name", "[]", "[]",
                                   "See /v1/catalog"));
            return;
        }
        if (!knownMetric(metric)) {
            sendResponse(fd, 404, "Not Found",
                         errorJson("metric_not_found", "Unknown metric", "[]", "[]",
                                   "See /v1/catalog"));
            return;
        }
        const IndexState idx = readIndexState(opt.indexPath);
        if (!idx.ready) {
            sendResponse(fd, 409, "Conflict",
                         errorJson("index_not_ready",
                                   "The analytics index has not been built",
                                   "[\"analytics\"]", availableModesJson(opt),
                                   "Run minima-analytics -index; retry when /v1/capabilities reports ready"));
            return;
        }
        const std::string body = metricsJson(opt.indexPath, metric);
        if (body.empty()) {
            sendResponse(fd, 404, "Not Found",
                         errorJson("metric_not_found", "Unknown metric", "[]", "[]",
                                   "See /v1/catalog"));
            return;
        }
        sendResponse(fd, 200, "OK", body);
        return;
    }

    const IndexState live = readIndexState(opt.indexPath);
    auto requireIndex = [&]() -> bool {
        if (live.ready) return true;
        sendResponse(fd, 409, "Conflict",
                     errorJson("index_not_ready", "The analytics index has not been built",
                               "[\"analytics\"]", availableModesJson(opt),
                               "Run minima-analytics -index; retry when /v1/capabilities reports ready"));
        return false;
    };

    if (path == "/v1/tokens" && requireIndex()) {
        bool found = false;
        const std::string body = tokensJson(opt.indexPath, found);
        sendResponse(fd, found ? 200 : 404, found ? "OK" : "Not Found",
                     found ? body : errorJson("not_found", "No token snapshot", "[]", "[]", ""));
        return;
    }
    if (path.rfind("/v1/tokens/", 0) == 0 && requireIndex()) {
        const std::string token = path.substr(11);
        bool found = false;
        const std::string body = assetUtxoJson(opt.indexPath, token, queryParam(query, "day"), found);
        sendResponse(fd, found ? 200 : 404, found ? "OK" : "Not Found",
                     found ? body : errorJson("not_found", "Unknown token", "[]", "[]", ""));
        return;
    }
    if (path == "/v1/snapshots" && requireIndex()) {
        bool found = false;
        const std::string body = snapshotsJson(opt.indexPath, found);
        sendResponse(fd, found ? 200 : 404, found ? "OK" : "Not Found",
                     found ? body : errorJson("not_found", "No snapshots", "[]", "[]", ""));
        return;
    }

    if (path.rfind("/v1/blocks/", 0) == 0 && requireIndex()) {
        const std::string suffix = path.substr(11);
        try {
            long long h = std::stoll(suffix);
            bool found = false;
            const std::string body = blockJson(opt.indexPath, h, found);
            if (!found) {
                sendResponse(fd, 404, "Not Found", errorJson("not_found", "Unknown block", "[]", "[]", ""));
            } else {
                sendResponse(fd, 200, "OK", body);
            }
        } catch (...) {
            sendResponse(fd, 400, "Bad Request", errorJson("invalid_height", "Invalid height", "[]", "[]", ""));
        }
        return;
    }
    if (path.rfind("/v1/txs/", 0) == 0 && requireIndex()) {
        const std::string txid = path.substr(8);
        bool found = false;
        const std::string body = txJson(opt.indexPath, txid, found);
        sendResponse(fd, found ? 200 : 404, found ? "OK" : "Not Found",
                     found ? body : errorJson("not_found", "Unknown transaction", "[]", "[]", ""));
        return;
    }
    if (path.rfind("/v1/addresses/", 0) == 0 && requireIndex()) {
        const std::string address = path.substr(14);
        bool found = false;
        const std::string body = addressJson(opt.indexPath, address,
                                             queryParam(query, "token"),
                                             queryParam(query, "day"), found);
        sendResponse(fd, found ? 200 : 404, found ? "OK" : "Not Found",
                     found ? body : errorJson("not_found", "Unknown address", "[]", "[]", ""));
        return;
    }
    if (path == "/v1/utxo/distribution" && requireIndex()) {
        const std::string token = queryParam(query, "token");
        bool found = false;
        const std::string body = token.empty()
            ? utxoJson(opt.indexPath, found)
            : assetUtxoJson(opt.indexPath, token, queryParam(query, "day"), found);
        sendResponse(fd, found ? 200 : 404, found ? "OK" : "Not Found",
                     found ? body : errorJson("not_found", "No UTXO snapshot", "[]", "[]", ""));
        return;
    }
    if (path == "/v1/cohorts/holder" && requireIndex()) {
        const std::string token = queryParam(query, "token");
        bool found = false;
        const std::string body = token.empty()
            ? cohortsJson(opt.indexPath, found)
            : assetCohortsJson(opt.indexPath, token, queryParam(query, "day"), found);
        sendResponse(fd, found ? 200 : 404, found ? "OK" : "Not Found",
                     found ? body : errorJson("not_found", "No cohort snapshot", "[]", "[]", ""));
        return;
    }
    if (path == "/v1/consistency/check" && requireIndex()) {
        bool found = false;
        const std::string body = consistencyJson(opt.indexPath, found);
        sendResponse(fd, found ? 200 : 404, found ? "OK" : "Not Found",
                     found ? body : errorJson("not_found", "No consistency check", "[]", "[]", ""));
        return;
    }

    if (path.rfind("/v1/", 0) == 0) {
        const bool needsMmr = path.rfind("/v1/utxo", 0) == 0 ||
                              path.rfind("/v1/cohorts", 0) == 0 ||
                              path.rfind("/v1/addresses", 0) == 0;
        const ArchiveStats archive = readArchiveStats(opt.archivePath);
        if (needsMmr && !std::filesystem::exists(opt.mmrPath) && !opt.forceMmr) {
            sendResponse(fd, 409, "Conflict",
                         errorJson("mode_unavailable",
                                   "This query requires a MegaMMR node",
                                   "[\"megammr\"]", availableModesJson(opt),
                                   "Run with -megammr or query an MMR-enabled endpoint"));
            return;
        }
        if (!needsMmr && !archive.exists && !opt.forceArchive) {
            sendResponse(fd, 409, "Conflict",
                         errorJson("mode_unavailable",
                                   "This query requires an archive node",
                                   "[\"archive\"]", availableModesJson(opt),
                                   "Run with -archive or query an archive endpoint"));
            return;
        }
        sendResponse(fd, 409, "Conflict",
                     errorJson("index_not_ready",
                               "The analytics index has not been built",
                               "[\"analytics\"]", availableModesJson(opt),
                               "Run the archive indexer; retry when /v1/capabilities reports ready"));
        return;
    }

    sendResponse(fd, 404, "Not Found",
                 errorJson("not_found", "Unknown endpoint", "[]", "[]",
                           "See /v1/capabilities"));
}

} // namespace

ApiServer::ApiServer(ApiOptions options) : mOptions(std::move(options)) {}

int ApiServer::run() {
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    const int listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0) {
        std::cerr << "socket failed: " << std::strerror(errno) << "\n";
        return 1;
    }
    int yes = 1;
    ::setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<std::uint16_t>(mOptions.port));

    if (::bind(listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind failed: " << std::strerror(errno) << "\n";
        ::close(listenFd);
        return 1;
    }
    if (::listen(listenFd, 32) < 0) {
        std::cerr << "listen failed: " << std::strerror(errno) << "\n";
        ::close(listenFd);
        return 1;
    }

    std::cout << "minima-analytics listening on 0.0.0.0:" << mOptions.port
              << " archive=" << mOptions.archivePath
              << " megammr=" << mOptions.mmrPath << std::endl;

    while (!gStop.load()) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listenFd, &readfds);
        timeval timeout{1, 0};
        const int ready = ::select(listenFd + 1, &readfds, nullptr, nullptr, &timeout);
        if (ready <= 0) continue;

        const int client = ::accept(listenFd, nullptr, nullptr);
        if (client < 0) continue;
        handleClient(client, mOptions);
        ::close(client);
    }

    ::close(listenFd);
    return 0;
}

} // namespace minima::analytics
