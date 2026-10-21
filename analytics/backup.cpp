#include "backup.hpp"

#include <duckdb.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace minima::analytics {

namespace {

std::string quoteSql(const std::string& value) {
    std::string out = "'";
    for (char c : value) {
        if (c == '\'') out += "''";
        else out += c;
    }
    out += "'";
    return out;
}

long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string jsonString(const duckdb::Value& value) {
    if (value.IsNull()) return "null";
    const std::string text = value.ToString();
    std::string out = "\"";
    for (char c : text) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    out += "\"";
    return out;
}

bool exec(duckdb::Connection& con, const std::string& sql, std::string& error) {
    auto res = con.Query(sql);
    if (!res || res->HasError()) {
        error = res ? res->GetError() : "query failed";
        return false;
    }
    return true;
}

} // namespace

BackupResult exportBackup(const BackupOptions& options) {
    BackupResult result;
    if (options.indexPath.empty() || !std::filesystem::exists(options.indexPath)) {
        result.error = "index database does not exist";
        return result;
    }
    if (options.backupDir.empty()) {
        result.error = "backup directory is empty";
        return result;
    }

    std::error_code ec;
    if (std::filesystem::exists(options.backupDir, ec)) {
        if (!std::filesystem::is_empty(options.backupDir, ec)) {
            result.error = "backup directory must be empty";
            return result;
        }
    } else {
        std::filesystem::create_directories(options.backupDir, ec);
        if (ec) {
            result.error = "cannot create backup directory";
            return result;
        }
    }

    try {
        duckdb::DuckDB db(options.indexPath);
        duckdb::Connection con(db);
        if (!exec(con, "CHECKPOINT;", result.error)) return result;
        if (!exec(con, "EXPORT DATABASE " + quoteSql(options.backupDir) + " (FORMAT PARQUET);",
                  result.error)) {
            return result;
        }

        auto idx = con.Query(
            "SELECT status,last_height,last_hash,archive_from,archive_to,tx_detail_from "
            "FROM analytics.index_state WHERE id=1;");
        auto mmr = con.Query(
            "SELECT root,coin_count FROM analytics.mmr_snapshots "
            "ORDER BY loaded_at_ms DESC LIMIT 1;");

        std::ostringstream manifest;
        manifest << "{\n"
                 << "  \"format\": \"minima-analytics-backup\",\n"
                 << "  \"version\": 1,\n"
                 << "  \"schema_version\": 1,\n"
                 << "  \"created_ms\": " << nowMs() << ",\n"
                 << "  \"export_format\": \"parquet\",\n";
        if (idx && !idx->HasError() && idx->RowCount() > 0) {
            result.height = idx->GetValue(1, 0).IsNull()
                                ? -1 : idx->GetValue(1, 0).GetValue<std::int64_t>();
            manifest << "  \"index\": {\"status\": " << jsonString(idx->GetValue(0, 0))
                     << ", \"last_height\": " << (result.height < 0 ? "null" : std::to_string(result.height))
                     << ", \"last_hash\": " << jsonString(idx->GetValue(2, 0))
                     << ", \"archive_from\": " << jsonString(idx->GetValue(3, 0))
                     << ", \"archive_to\": " << jsonString(idx->GetValue(4, 0))
                     << ", \"tx_detail_from\": " << jsonString(idx->GetValue(5, 0)) << "},\n";
        } else {
            manifest << "  \"index\": null,\n";
        }
        if (mmr && !mmr->HasError() && mmr->RowCount() > 0) {
            manifest << "  \"mmr\": {\"root\": " << jsonString(mmr->GetValue(0, 0))
                     << ", \"coins\": " << jsonString(mmr->GetValue(1, 0)) << "}\n";
        } else {
            manifest << "  \"mmr\": null\n";
        }
        manifest << "}\n";

        std::ofstream out(options.backupDir + "/analytics_manifest.json");
        if (!out) {
            result.error = "cannot write manifest";
            return result;
        }
        out << manifest.str();
        out.close();

        result.ok = true;
        return result;
    } catch (const std::exception& ex) {
        result.error = ex.what();
        return result;
    }
}

BackupResult importBackup(const BackupOptions& options) {
    BackupResult result;
    if (options.backupDir.empty() || !std::filesystem::exists(options.backupDir)) {
        result.error = "backup directory does not exist";
        return result;
    }
    if (options.indexPath.empty()) {
        result.error = "index database path is empty";
        return result;
    }
    std::error_code ec;
    if (std::filesystem::exists(options.indexPath, ec) &&
        std::filesystem::file_size(options.indexPath, ec) > 0) {
        result.error = "target index already exists; choose a new -indexdb";
        return result;
    }
    const auto parent = std::filesystem::path(options.indexPath).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);

    try {
        duckdb::DuckDB db(options.indexPath);
        duckdb::Connection con(db);
        if (!exec(con, "IMPORT DATABASE " + quoteSql(options.backupDir) + ";", result.error)) {
            return result;
        }
        auto res = con.Query("SELECT status,last_height FROM analytics.index_state WHERE id=1;");
        if (!res || res->HasError() || res->RowCount() == 0) {
            result.error = "restored index has no index_state";
            return result;
        }
        if (!res->GetValue(1, 0).IsNull()) {
            result.height = res->GetValue(1, 0).GetValue<std::int64_t>();
        }
        result.ok = true;
        return result;
    } catch (const std::exception& ex) {
        result.error = ex.what();
        return result;
    }
}

} // namespace minima::analytics
