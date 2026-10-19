# RFC-0003 — embedded engine benchmark

Host: 64-bit · dataset: 200000 blocks / 120000 coins · lookups: 3000 · scan span: 1000

Engines built: `sqlite`, `sqlite-plain`, `lmdb`, `rocksdb`

## archive

| metric | sqlite | sqlite-plain | lmdb | rocksdb | 
|---|---|---|---|---|
| bulk_load_rows_per_s | 85140 rows/s | 188 rows/s | 206447 rows/s | 183366 rows/s | 
| point_get_by_num_p50_us | 6.46 us | 8.71 us | 1.71 us | 3.62 us | 
| point_get_by_num_p95_us | 8.24 us | 10.64 us | 4.95 us | 5.60 us | 
| point_get_by_id_p50_us | 3.55 us | 6.03 us | 2.86 us | 7.43 us | 
| scan_asc_ms | 0 ms | 0 ms | 0 ms | 0 ms | 
| scan_desc_ms | 0 ms | 0 ms | 0 ms | 1 ms | 
| count_rows | 200000 | 200000 | 200000 | 200000 | 
| first_block | 0 | 0 | 0 | 0 | 
| last_block | 199999 | 199999 | 199999 | 199999 | 
| analytics_ms | 52 ms | 83 ms | 27 ms | 61 ms | 
| analytics_txns | 4106733 | 4106733 | 4106733 | 4106733 | 
| snapshot_s | 1 s | 0 s | 0 s | 1 s | 
| peak_rss_mb | 424 MB | 269 MB | 571 MB | 505 MB | 
| on_disk_mb | 153 MB | 153 MB | 183 MB | 135 MB | 

## coin

| metric | sqlite | sqlite-plain | lmdb | rocksdb | 
|---|---|---|---|---|
| bulk_load_rows_per_s | 173643 rows/s | 193 rows/s | 279742 rows/s | 138229 rows/s | 
| point_get_p50_us | 4.92 us | 11.10 us | 1.26 us | 3.25 us | 
| point_get_p95_us | 11.91 us | 12.45 us | 3.73 us | 4.94 us | 
| prefix_scan_ms | 0 ms | 0 ms | 0 ms | 0 ms | 
| prefix_scan_hits | 1 | 1 | 1 | 1 | 
| count | 120000 | 120000 | 120000 | 120000 | 
| snapshot_s | 0 s | 0 s | 0 s | 0 s | 
| peak_rss_mb | 314 MB | 269 MB | 327 MB | 370 MB | 
| on_disk_mb | 44 MB | 44 MB | 58 MB | 38 MB | 

Raw: `raw.tsv`. Methodology and fairness statement: `README.md`.
