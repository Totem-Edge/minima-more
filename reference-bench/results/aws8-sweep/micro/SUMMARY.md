# Reference benchmark summary

Run: /tmp/aws8-sweep/micro
Java: minima-core `c2a54a2` (performance reference only — NOT byte-parity).

| Metric | Java (minima-core) | C++ (this node) |
|---|---|---|
| WOTS pubkey (ms/op) | 9.920168 | 2.231226 |
| WOTS sign (ms/op, batched) | 5.240560 | 1.072981 |
| WOTS verify (ms/op) | 4.624736 | 1.164362 |
| SHA3-256 224B (h/s, 1 thr) | 283025.992917 | 967125.566394 |
| MMR addEntry+getRoot (entries/s) | 121643.513344 | 214359.526692 |

C++ WOTS sign scalar vs batched:
    scalar   3.89389 ms/sign  (1946.94 ms)
    batched  1.24704 ms/sign  (623.519 ms)
    speedup  3.12251x
    identical yes

Raw files: environment.txt, java_bench.txt, cpp_sha3.txt, cpp_wots_signature.txt, cpp_bench_core.txt
