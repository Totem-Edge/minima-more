# Reference benchmark summary

Run: /home/ubuntu/pm/src/reference-bench/results/a1-full-20261018-061142/micro
Java: minima-core `c2a54a2` (performance reference only — NOT byte-parity).

| Metric | Java (minima-core) | C++ (this node) |
|---|---|---|
| WOTS pubkey (ms/op) | 8.127999 | 1.904691 |
| WOTS sign (ms/op, batched) | 4.298728 | 0.919870 |
| WOTS verify (ms/op) | 3.782524 | 1.027661 |
| SHA3-256 224B (h/s, 1 thr) | 346289.413291 | 1158839.287472 |
| MMR addEntry+getRoot (entries/s) | 139109.074403 | 259839.258498 |

C++ WOTS sign scalar vs batched:
    scalar   3.26273 ms/sign  (1631.36 ms)
    batched  1.06938 ms/sign  (534.688 ms)
    speedup  3.05106x
    identical yes

Raw files: environment.txt, java_bench.txt, cpp_sha3.txt, cpp_wots_signature.txt, cpp_bench_core.txt
