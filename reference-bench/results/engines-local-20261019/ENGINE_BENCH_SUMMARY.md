# RFC-0003 — embedded engine benchmark

Host: 64-bit · dataset: 30000 blocks / 20000 coins · lookups: 2000 · scan span: 5000

Engines built: `sqlite`, `sqlite-plain`, `lmdb`, `rocksdb`, `duckdb`

## archive

| metric | sqlite | sqlite-plain | lmdb | rocksdb | duckdb | 
|---|---|---|---|---|---|
| bulk_load_rows_per_s | 302412 rows/s | 1932 rows/s | 286444 rows/s | 265100 rows/s | 102249 rows/s | 
| point_get_by_num_p50_us | 2.33 us | 6.67 us | 1.38 us | 1.67 us | 886.54 us | 
| point_get_by_num_p95_us | 2.79 us | 10.75 us | 1.96 us | 8.79 us | 1204.79 us | 
| point_get_by_id_p50_us | 1.42 us | 2.92 us | 1.42 us | 2.92 us | 85.21 us | 
| scan_asc_ms | 0 ms | 1 ms | 0 ms | 1 ms | 4 ms | 
| scan_desc_ms | 0 ms | 0 ms | 0 ms | 1 ms | 0 ms | 
| count_rows | 30000 | 30000 | 30000 | 30000 | 30000 | 
| first_block | 0 | 0 | 0 | 0 | 0 | 
| last_block | 29999 | 29999 | 29999 | 29999 | 29999 | 
| analytics_ms | 2 ms | 6 ms | 1 ms | 4 ms | 12 ms | 
| analytics_txns | 612182 | 612182 | 612182 | 612182 | 612182 | 
| snapshot_s | 0 s | 0 s | 0 s | 0 s | 0 s | 
| peak_rss_mb | 122 MB | 49 MB | 97 MB | 145 MB | 192 MB | 
| on_disk_mb | 22 MB | 22 MB | 22 MB | 20 MB | 22 MB | 

## coin

| metric | sqlite | sqlite-plain | lmdb | rocksdb | duckdb | 
|---|---|---|---|---|---|
| bulk_load_rows_per_s | 339034 rows/s | 2608 rows/s | 386829 rows/s | 210975 rows/s | 493950 rows/s | 
| point_get_p50_us | 2.08 us | 6.75 us | 0.71 us | 0.87 us | 158.79 us | 
| point_get_p95_us | 3.17 us | 10.12 us | 1.71 us | 1.46 us | 202.88 us | 
| prefix_scan_ms | 0 ms | 0 ms | 0 ms | 0 ms | 0 ms | 
| prefix_scan_hits | 1 | 1 | 1 | 1 | 1 | 
| count | 20000 | 20000 | 20000 | 20000 | 20000 | 
| snapshot_s | 0 s | 0 s | 0 s | 0 s | 0 s | 
| peak_rss_mb | 107 MB | 51 MB | 82 MB | 146 MB | 193 MB | 
| on_disk_mb | 7 MB | 7 MB | 9 MB | 6 MB | 7 MB | 

Raw: `raw.tsv`. Methodology and fairness statement: `README.md`.
