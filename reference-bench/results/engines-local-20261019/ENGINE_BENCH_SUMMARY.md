# RFC-0003 — embedded engine benchmark

Host: 64-bit · dataset: 100000 blocks / 60000 coins · lookups: 2000 · scan span: 5000

Engines built: `sqlite`, `lmdb`

## archive

| metric | sqlite | lmdb | 
|---|---|---|
| bulk_load_rows_per_s | 249658 rows/s | 540246 rows/s | 
| point_get_by_num_p50_us | 2 us | 1 us | 
| point_get_by_num_p95_us | 3 us | 1 us | 
| point_get_by_id_p50_us | 2 us | 3 us | 
| scan_asc_ms | 0 ms | 0 ms | 
| scan_desc_ms | 0 ms | 0 ms | 
| count_rows | 100000 | 100000 | 
| first_block | 0 | 0 | 
| last_block | 99999 | 99999 | 
| analytics_ms | 11 ms | 5 ms | 
| analytics_txns | 2048075 | 2048075 | 
| snapshot_s | 0 s | 0 s | 
| peak_rss_mb | 333 MB | 374 MB | 
| on_disk_mb | 76 MB | 75 MB | 

## coin

| metric | sqlite | lmdb | 
|---|---|---|
| bulk_load_rows_per_s | 334182 rows/s | 258039 rows/s | 
| point_get_p50_us | 2 us | 1 us | 
| point_get_p95_us | 4 us | 2 us | 
| prefix_scan_ms | 0 ms | 0 ms | 
| prefix_scan_hits | 1 | 1 | 
| count | 60000 | 60000 | 
| snapshot_s | 0 s | 0 s | 
| peak_rss_mb | 284 MB | 285 MB | 
| on_disk_mb | 22 MB | 28 MB | 

Raw: `raw.tsv`. Methodology and fairness statement: `README.md`.
