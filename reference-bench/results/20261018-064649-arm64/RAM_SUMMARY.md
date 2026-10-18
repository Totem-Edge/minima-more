# RAM-scaling comparison (MMR, N=1000000 entries)

C++ = this node | Java = minima-core `c2a54a2` (performance reference only).

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| bytes/MMR entry | 684.65 | 602.201776 (heap) |
| delta for 1000000 entries (kB) | 668608 | 588087 (heap) |
| process peak RSS (kB) | 670528 | 1696976 |

Caveats: C++ is process RSS; Java rows are JVM heap-in-use (plus the peak
RSS row which is the whole JVM). Heap != RSS. Single run, indicative only.
