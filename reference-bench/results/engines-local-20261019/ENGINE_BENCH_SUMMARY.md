# RFC-0003 — embedded engine benchmark

Host: 64-bit · dataset: 200000 blocks / 120000 coins · lookups: 3000 · scan span: 10000

Engines built: `sqlite`, `lmdb`, `rocksdb`, `duckdb`

## archive

| metric | sqlite | lmdb | rocksdb | duckdb | 
|---|---|---|---|---|
| bulk_load_rows_per_s | 163971 rows/s | 467569 rows/s | 377822 rows/s | 136040 rows/s | 
| point_get_by_num_p50_us | 2.71 us | 1.92 us | 3.79 us | 312.62 us | 
| point_get_by_num_p95_us | 3.42 us | 2.50 us | 11.33 us | 971.50 us | 
| point_get_by_id_p50_us | 2.67 us | 3.75 us | 5.46 us | 107.92 us | 
| scan_asc_ms | 2 ms | 0 ms | 2 ms | 5 ms | 
| scan_desc_ms | 1 ms | 0 ms | 4 ms | 1 ms | 
| count_rows | 200000 | 200000 | 200000 | 200000 | 
| first_block | 0 | 0 | 0 | 0 | 
| last_block | 199999 | 199999 | 199999 | 199999 | 
| analytics_ms | 23 ms | 14 ms | 35 ms | 2 ms | 
| analytics_txns | 4106733 | 4106733 | 4106733 | 4106733 | 
| snapshot_s | 0 s | 0 s | 0 s | 0 s | 
| peak_rss_mb | 463 MB | 651 MB | 884 MB | 632 MB | 
| on_disk_mb | 153 MB | 150 MB | 135 MB | 145 MB | 

## coin

| metric | sqlite | lmdb | rocksdb | duckdb | 
|---|---|---|---|---|
| bulk_load_rows_per_s | 261733 rows/s | 319176 rows/s | 179001 rows/s | 328214 rows/s | 
| point_get_p50_us | 3.29 us | 1.75 us | 2.08 us | 578.75 us | 
| point_get_p95_us | 4.54 us | 2.46 us | 4.96 us | 1172.62 us | 
| prefix_scan_ms | 0 ms | 0 ms | 0 ms | 1 ms | 
| prefix_scan_hits | 1 | 1 | 1 | 1 | 
| count | 120000 | 120000 | 120000 | 120000 | 
| snapshot_s | 0 s | 0 s | 0 s | 0 s | 
| peak_rss_mb | 397 MB | 493 MB | 680 MB | 663 MB | 
| on_disk_mb | 44 MB | 56 MB | 38 MB | 42 MB | 

Raw: `raw.tsv`. Methodology and fairness statement: `README.md`.
