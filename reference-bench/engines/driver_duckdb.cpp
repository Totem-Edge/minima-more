// RFC-0003 — DuckDB driver (role R1 archive analytics + R2 `duckdbcoins`).
//
// STATUS: scaffold. DuckDB is not installed on the dev host; it is expected to
// lose point lookups and win analytics (RFC-0003 §8), so the harness must make
// that split explicit. Build it on the A1 lab host.
//
// Implementation notes for when `find duckdb` succeeds:
//  - Archive: columnar table `blocks(num BIGINT, id BLOB, txns BIGINT, payload BLOB)`.
//    Bulk load via the Appender API (the whole point of DuckDB), not INSERTs.
//  - Point get: parameterised query (expect ~ms/statement overhead vs SQLite's
//    ~µs) — this is the datapoint RFC-0001 §4.1 predicts.
//  - Analytics: run the same SUM/COUNT aggregate forms natively (columnar).
//  - Coin: table `coins(coinid BLOB, addr BLOB, amount BIGINT, proof BLOB)`;
//    ART index for point get; prefix scan via a range predicate.
//  - snapshot: `EXPORT DATABASE` / `COPY ... (FORMAT PARQUET)`.
#include "interface.hpp"

namespace eng {

std::unique_ptr<ArchiveStore> make_duckdb_archive() { return nullptr; }
std::unique_ptr<CoinStore> make_duckdb_coin() { return nullptr; }

}  // namespace eng
