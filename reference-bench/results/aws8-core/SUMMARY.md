# Reference benchmark summary

Run: /tmp/aws8-core
Java: minima-core `c2a54a2` (performance reference only — NOT byte-parity).

| Metric | Java (minima-core) | C++ (this node) |
|---|---|---|
| WOTS pubkey (ms/op) | 9.911725 | 2.253653 |
| WOTS sign (ms/op, batched) | 5.240155 | 1.096914 |
| WOTS verify (ms/op) | 4.620318 | 1.189860 |
| SHA3-256 224B (h/s, 1 thr) | 283461.489457 | 966765.492980 |
| MMR addEntry+getRoot (entries/s) | 119834.110286 | 212421.109853 |

C++ WOTS sign scalar vs batched:
    scalar   3.87942 ms/sign  (1939.71 ms)
    batched  1.23687 ms/sign  (618.435 ms)
    speedup  3.13648x
    identical yes

Raw files: environment.txt, java_bench.txt, cpp_sha3.txt, cpp_wots_signature.txt, cpp_bench_core.txt
