# Reference benchmark summary

Run: /Users/peter.cheek/Downloads/CRYSTAL_LABS/SIDE_Projects/pureminima_cpp pre-totem post-opt/reference-bench/results/20261016-162107-arm64
Java: minima-core `c2a54a2` (performance reference only — NOT byte-parity).

| Metric | Java (minima-core) | C++ (this node) |
|---|---|---|
| WOTS pubkey (ms/op) | 5.303470 | 1.112073 |
| WOTS sign (ms/op, batched) | 2.884255 | 0.627816 |
| WOTS verify (ms/op) | 2.212798 | 0.584588 |
| SHA3-256 224B (h/s, 1 thr) | 569427.846018 | 2483883.224344 |
| MMR addEntry+getRoot (entries/s) | 171263.913898 | 290016.544429 |

C++ WOTS sign scalar vs batched:
    scalar   2.78402 ms/sign  (1392.01 ms)
    batched  1.30974 ms/sign  (654.871 ms)
    speedup  2.12562x
    identical yes

Raw files: environment.txt, java_bench.txt, cpp_sha3.txt, cpp_wots_signature.txt
