#pragma once

#include <cstddef>

namespace minima::analytics {

struct MetricDef {
    const char* name;
    const char* unit;
    const char* granularity;
    const char* source;
    const char* minMode;
    const char* description;
};

inline constexpr MetricDef kMetricCatalog[] = {
    {"block_height", "count", "1m/1h/1d", "live", "full", "Max block height at bucket end"},
    {"block_count", "count", "1m/1h/1d", "archive", "archive", "Blocks in bucket"},
    {"tx_count", "count", "1m/1h/1d", "archive", "archive", "Transactions in bucket"},
    {"tx_rate", "tx/s", "1m/1h", "archive", "archive", "Transactions per second"},
    {"fees_total_raw", "raw", "1h/1d", "archive", "archive", "Sum of transaction fees"},
    {"coinbase_reward_raw", "raw", "1d", "archive", "archive", "Sum of coinbase outputs"},
    {"output_volume_raw", "raw", "1h/1d", "archive", "archive", "Sum of non-coinbase outputs"},
    {"input_volume_raw", "raw", "1h/1d", "archive", "archive", "Sum of spent inputs"},
    {"active_addresses", "count", "1h/1d", "archive", "archive", "Unique input or output addresses"},
    {"new_addresses", "count", "1d", "archive", "archive", "First-seen output addresses"},
    {"sender_addresses", "count", "1d", "archive", "archive", "Unique input addresses"},
    {"receiver_addresses", "count", "1d", "archive", "archive", "Unique output addresses"},
    {"utxo_count", "count", "snapshot", "megammr", "megammr", "Unspent coins at as_of"},
    {"supply_raw", "raw", "snapshot", "megammr", "megammr", "Unspent native supply at as_of"},
    {"spent_coin_count", "count", "1d", "archive", "archive", "Coins first spent in bucket"},
    {"utxo_age_band_count", "count", "snapshot", "archive", "archive", "Unspent coins per age band"},
    {"utxo_age_band_value_raw", "raw", "snapshot", "archive", "archive", "Value per age band"},
    {"utxo_value_band_count", "count", "snapshot", "megammr", "megammr", "Coins per value band"},
    {"holder_cohort_count", "count", "snapshot", "megammr", "megammr", "Addresses per balance cohort"},
    {"holder_cohort_value_raw", "raw", "snapshot", "megammr", "megammr", "Balance per cohort"},
    {"address_inflow_raw", "raw", "1d", "archive", "archive", "Value received by address"},
    {"address_outflow_raw", "raw", "1d", "archive", "archive", "Value spent by address"},
    {"blocks_mined", "count", "1d", "archive", "archive", "Blocks by minter"},
    {"index_lag_blocks", "count", "1m", "index", "analytics", "Live tip minus last indexed height"},
    {"archive_mmr_divergence", "count", "snapshot", "index", "analytics", "UTXO mismatches at check height"},
    {"spent_output_age_avg_days", "days", "1d", "archive", "archive", "Value-weighted average spent-output age"},
    {"spent_output_age_median_days", "days", "1d", "archive", "archive", "Median spent-output age"},
    {"cdd", "coin-days", "1d", "archive", "archive", "Coin days destroyed"},
    {"cdd_90", "coin-days", "1d", "archive", "archive", "90-day rolling CDD"},
    {"cyd", "coin-days", "1d", "archive", "archive", "365-day rolling CDD"},
    {"dormancy_days", "days", "1d", "archive", "archive", "Average dormancy of spent coins"},
    {"utxo_value_p50_raw", "raw", "snapshot", "archive", "archive", "Median UTXO value"},
    {"utxo_value_p90_raw", "raw", "snapshot", "archive", "archive", "90th percentile UTXO value"},
    {"utxo_value_p99_raw", "raw", "snapshot", "archive", "archive", "99th percentile UTXO value"},
    {"reorg_depth", "count", "event", "live", "full", "Deepest recent reorg depth"},
};

inline constexpr std::size_t kMetricCatalogSize =
    sizeof(kMetricCatalog) / sizeof(kMetricCatalog[0]);

} // namespace minima::analytics
