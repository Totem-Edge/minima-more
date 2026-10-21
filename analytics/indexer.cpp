#include "indexer.hpp"

#include "archive_reader.hpp"

#include <duckdb.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

namespace minima::analytics {

namespace {

std::string readFile(const std::string& path, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "cannot open " + path;
        return {};
    }
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

bool execScript(duckdb::Connection& con, const std::string& script, std::string& error) {
    std::string cleaned;
    cleaned.reserve(script.size());
    bool inQuote = false;
    bool lineComment = false;
    bool blockComment = false;
    for (std::size_t i = 0; i < script.size(); ++i) {
        const char c = script[i];
        const char next = i + 1 < script.size() ? script[i + 1] : '\0';
        if (lineComment) {
            if (c == '\n') {
                lineComment = false;
                cleaned += c;
            }
            continue;
        }
        if (blockComment) {
            if (c == '*' && next == '/') {
                blockComment = false;
                ++i;
            }
            continue;
        }
        if (!inQuote && c == '-' && next == '-') {
            lineComment = true;
            ++i;
            continue;
        }
        if (!inQuote && c == '/' && next == '*') {
            blockComment = true;
            ++i;
            continue;
        }
        if (c == '\'') inQuote = !inQuote;
        cleaned += c;
    }

    std::string statement;
    inQuote = false;
    for (char c : cleaned) {
        if (c == '\'') inQuote = !inQuote;
        if (c == ';' && !inQuote) {
            bool hasContent = false;
            for (char sc : statement) {
                if (sc != ' ' && sc != '\n' && sc != '\r' && sc != '\t') hasContent = true;
            }
            if (hasContent) {
                auto res = con.Query(statement);
                if (!res || res->HasError()) {
                    error = res ? res->GetError() : "query failed";
                    return false;
                }
            }
            statement.clear();
        } else {
            statement += c;
        }
    }
    if (!statement.empty()) {
        auto res = con.Query(statement);
        if (!res || res->HasError()) {
            error = res ? res->GetError() : "query failed";
            return false;
        }
    }
    return true;
}

bool exec(duckdb::Connection& con, const std::string& sql, std::string& error) {
    auto res = con.Query(sql);
    if (!res || res->HasError()) {
        error = res ? res->GetError() : "query failed";
        return false;
    }
    return true;
}

std::string addr(const org::minima::objects::Coin& coin) {
    return coin.getAddress().to0xString();
}

std::string token(const org::minima::objects::Coin& coin) {
    return coin.getTokenID().to0xString();
}

long long createdHeight(const org::minima::objects::Coin& coin, long long fallback) {
    const long long v = coin.getBlockCreated().getAsLong();
    return v > 0 ? v : fallback;
}

} // namespace

IndexResult buildIndex(const IndexOptions& options) {
    IndexResult result;
    if (options.rebuild) {
        std::error_code ec;
        std::filesystem::remove(options.indexPath, ec);
        std::filesystem::remove(options.indexPath + ".wal", ec);
    }

    std::string error;
    const std::string schema = readFile(options.schemaPath, error);
    if (schema.empty()) {
        result.error = error;
        return result;
    }

    duckdb::DuckDB db(options.indexPath);
    duckdb::Connection con(db);
    if (!execScript(con, schema, error)) {
        result.error = error;
        return result;
    }

    long long lastHeight = 0;
    {
        auto res = con.Query("SELECT COALESCE(MAX(height),0) FROM analytics.chain_blocks;");
        if (!res || res->HasError()) {
            result.error = res ? res->GetError() : "index_state query failed";
            return result;
        }
        lastHeight = res->GetValue(0, 0).GetValue<std::int64_t>();
    }

    duckdb::Appender blocks(con, "analytics", "chain_blocks");
    duckdb::Appender txs(con, "analytics", "txs");
    duckdb::Appender inputs(con, "analytics", "tx_inputs");
    duckdb::Appender outputs(con, "analytics", "tx_outputs");
    duckdb::Appender coins(con, "analytics", "coins");

    long long processed = 0;
    std::string lastHash;

    const bool ok = forEachArchiveBlock(
        options.archivePath, lastHeight,
        [&](std::int64_t height, const std::vector<std::uint8_t>& raw) {
            org::minima::objects::base::MiniData md(raw);
            auto block = org::minima::objects::TxBlock::convertMiniDataVersion(md);
            if (!block) {
                result.error = "failed to decode block " + std::to_string(height);
                return false;
            }
            const auto& pow = block->getTxPoW();
            const std::string hash = pow.getTxPoWID();
            const std::string prev = pow.getParentID().to0xString();
            const long long timeMs = pow.getTimeMilli().getAsLong();

            int txCount = 0;
            std::string txid = hash;
            int inCount = static_cast<int>(block->getInputCoinProofs().size());
            int outCount = static_cast<int>(block->getOutputCoins().size());
            try {
                const auto& tx = pow.getTransaction();
                if (!tx.isEmpty()) {
                    txCount = 1;
                    const std::string id = tx.getTransactionID().to0xString();
                    if (!id.empty()) txid = id;
                }
            } catch (...) {
                txCount = 0;
            }

            blocks.AppendRow(
                static_cast<std::int64_t>(height), hash.c_str(), prev.c_str(), static_cast<std::int64_t>(timeMs),
                static_cast<int>(txCount), static_cast<std::int64_t>(raw.size()),
                duckdb::Value(), duckdb::Value(), duckdb::Value(), true);

            txs.AppendRow(txid.c_str(), static_cast<std::int64_t>(height), 0, static_cast<std::int64_t>(timeMs), false,
                          duckdb::Value(), inCount, outCount,
                          static_cast<std::int64_t>(raw.size()), duckdb::Value(), duckdb::Value());

            int inputIndex = 0;
            for (const auto& proof : block->getInputCoinProofs()) {
                const auto& coin = proof.getCoin();
                const std::string cid = coin.getCoinID().to0xString();
                const std::string cad = addr(coin);
                const std::string ctk = token(coin);
                inputs.AppendRow(txid.c_str(), inputIndex++, cid.c_str(),
                                 cad.c_str(), ctk.c_str(),
                                 static_cast<std::int64_t>(coin.getAmount().getAsLong()),
                                 static_cast<std::int64_t>(createdHeight(coin, height)));
            }

            int outputIndex = 0;
            for (const auto& coin : block->getOutputCoins()) {
                const std::string coinId = coin.getCoinID().to0xString();
                const long long created = createdHeight(coin, height);
                const std::string cad = addr(coin);
                const std::string ctk = token(coin);
                outputs.AppendRow(txid.c_str(), outputIndex++, coinId.c_str(), cad.c_str(), ctk.c_str(),
                                  static_cast<std::int64_t>(coin.getAmount().getAsLong()),
                                  duckdb::Value(), duckdb::Value(), static_cast<std::int64_t>(created),
                                  duckdb::Value(), duckdb::Value());
                coins.AppendRow(coinId.c_str(), cad.c_str(), ctk.c_str(),
                                static_cast<std::int64_t>(coin.getAmount().getAsLong()),
                                static_cast<std::int64_t>(created), static_cast<std::int64_t>(timeMs), duckdb::Value(), duckdb::Value(),
                                duckdb::Value(), duckdb::Value(), duckdb::Value(),
                                false, duckdb::Value());
            }

            lastHash = hash;
            ++processed;
            return true;
        },
        error);

    blocks.Close();
    txs.Close();
    inputs.Close();
    outputs.Close();
    coins.Close();

    if (!ok) {
        result.error = error;
        return result;
    }

    // Recompute the archive-backed time series from the indexed tables.
    const char* clearSeries =
        "DELETE FROM analytics.metric_series WHERE source = 'archive';";
    if (!exec(con, clearSeries, error)) {
        result.error = error;
        return result;
    }

    const std::vector<std::string> metricSql = {
        "INSERT INTO analytics.metric_series "
        "(metric,granularity,bucket_start_ms,height,value,value_raw,source,complete) "
        "SELECT 'block_count','1d',CAST(timestamp_ms/86400000 AS BIGINT)*86400000,"
        "MAX(height),COUNT(*),NULL,'archive',true FROM analytics.chain_blocks GROUP BY 3;",
        "INSERT INTO analytics.metric_series "
        "(metric,granularity,bucket_start_ms,height,value,value_raw,source,complete) "
        "SELECT 'tx_count','1d',CAST(timestamp_ms/86400000 AS BIGINT)*86400000,"
        "MAX(height),SUM(tx_count),NULL,'archive',true FROM analytics.chain_blocks GROUP BY 3;",
        "INSERT INTO analytics.metric_series "
        "(metric,granularity,bucket_start_ms,height,value,value_raw,source,complete) "
        "SELECT 'input_volume_raw','1d',CAST(b.timestamp_ms/86400000 AS BIGINT)*86400000,"
        "MAX(b.height),NULL,SUM(i.amount_raw),'archive',true "
        "FROM analytics.tx_inputs i JOIN analytics.txs t ON t.txpowid=i.txpowid "
        "JOIN analytics.chain_blocks b ON b.height=t.height GROUP BY 3;",
        "INSERT INTO analytics.metric_series "
        "(metric,granularity,bucket_start_ms,height,value,value_raw,source,complete) "
        "SELECT 'output_volume_raw','1d',CAST(b.timestamp_ms/86400000 AS BIGINT)*86400000,"
        "MAX(b.height),NULL,SUM(o.amount_raw),'archive',true "
        "FROM analytics.tx_outputs o JOIN analytics.txs t ON t.txpowid=o.txpowid "
        "JOIN analytics.chain_blocks b ON b.height=t.height GROUP BY 3;",
        "INSERT INTO analytics.metric_series "
        "(metric,granularity,bucket_start_ms,height,value,value_raw,source,complete) "
        "SELECT 'spent_coin_count','1d',CAST(b.timestamp_ms/86400000 AS BIGINT)*86400000,"
        "MAX(b.height),COUNT(*),NULL,'archive',true "
        "FROM analytics.tx_inputs i JOIN analytics.txs t ON t.txpowid=i.txpowid "
        "JOIN analytics.chain_blocks b ON b.height=t.height GROUP BY 3;",
        "INSERT INTO analytics.metric_series "
        "(metric,granularity,bucket_start_ms,height,value,value_raw,source,complete) "
        "WITH ev AS ("
        " SELECT CAST(b.timestamp_ms/86400000 AS BIGINT)*86400000 d, b.height h, i.address addr"
        " FROM analytics.tx_inputs i JOIN analytics.txs t ON t.txpowid=i.txpowid"
        " JOIN analytics.chain_blocks b ON b.height=t.height"
        " UNION ALL"
        " SELECT CAST(b.timestamp_ms/86400000 AS BIGINT)*86400000, b.height, o.address"
        " FROM analytics.tx_outputs o JOIN analytics.txs t ON t.txpowid=o.txpowid"
        " JOIN analytics.chain_blocks b ON b.height=t.height)"
        " SELECT 'active_addresses','1d',d,MAX(h),COUNT(DISTINCT addr),NULL,'archive',true"
        " FROM ev GROUP BY d;",
        "INSERT INTO analytics.metric_series "
        "(metric,granularity,bucket_start_ms,height,value,value_raw,source,complete) "
        "SELECT 'utxo_count','snapshot',MAX(b.timestamp_ms),MAX(b.height),COUNT(*),NULL,"
        "'archive',true FROM analytics.tx_outputs o "
        "JOIN analytics.chain_blocks b ON b.height=(SELECT MAX(height) FROM analytics.chain_blocks) "
        "WHERE NOT EXISTS (SELECT 1 FROM analytics.tx_inputs i WHERE i.coin_id=o.coin_id);",
        "INSERT INTO analytics.metric_series "
        "(metric,granularity,bucket_start_ms,height,value,value_raw,source,complete) "
        "SELECT 'supply_raw','snapshot',MAX(b.timestamp_ms),MAX(b.height),NULL,"
        "SUM(o.amount_raw),'archive',true FROM analytics.tx_outputs o "
        "JOIN analytics.chain_blocks b ON b.height=(SELECT MAX(height) FROM analytics.chain_blocks) "
        "WHERE o.token_id='0x00' AND NOT EXISTS "
        "(SELECT 1 FROM analytics.tx_inputs i WHERE i.coin_id=o.coin_id);",
    };
    for (const auto& sql : metricSql) {
        if (!exec(con, sql, error)) {
            result.error = error;
            return result;
        }
    }

    const long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();
    std::ostringstream upsert;
    upsert << "INSERT INTO analytics.index_state "
              "(id,schema_version,last_height,last_hash,archive_from,archive_to,tx_detail_from,"
              "mmr_as_of,mmr_hash,lag_blocks,status,updated_at_ms) VALUES "
              "(1,1," << (processed > 0 ? (lastHeight + processed) : lastHeight) << ","
           << (lastHash.empty() ? std::string("NULL") : "'" + lastHash + "'") << ",1,"
           << (processed > 0 ? (lastHeight + processed) : lastHeight) << ",1,NULL,NULL,0,'ready',"
           << now << ") ON CONFLICT (id) DO UPDATE SET "
              "last_height=excluded.last_height,last_hash=excluded.last_hash,"
              "archive_to=excluded.archive_to,lag_blocks=0,status='ready',"
              "updated_at_ms=excluded.updated_at_ms;";
    if (!exec(con, upsert.str(), error)) {
        result.error = error;
        return result;
    }

    result.ok = true;
    result.blocks = processed;
    return result;
}

} // namespace minima::analytics
