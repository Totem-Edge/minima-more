# RAM-scaling comparison (MMR, N=1000000 entries)

C++ = this node | Java = minima-core `c2a54a2` (performance reference only).

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| bytes/MMR entry | 633.89 | 601.859424 (heap) |
| delta for 1000000 entries (kB) | 619036 | 587753 (heap) |
| process peak RSS (kB) | 623872 | 1897856 |

Caveats: C++ is process RSS; Java rows are JVM heap-in-use (plus the peak
RSS row which is the whole JVM). Heap != RSS. Single run, indicative only.
