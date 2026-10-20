# Reference performance benchmarks

This directory records performance comparisons between this C++ node and the
**Java reference node** [`spartacusrex-minima/minima-core`](https://github.com/spartacusrex-minima/minima-core),
so the optimisation journey and its outcome can be presented end to end.

## ⚠️ Scope: performance only — NOT a byte-parity oracle

`minima-core` is used **only** as a performance baseline. It must **not** be used
for consensus/byte-parity checks: it may have implemented a **different address
derivation path**, so keys/addresses/signatures are not guaranteed to match this
node byte-for-byte.

Correctness/byte-parity of this node is anchored elsewhere and independently:
- `tests/test_consensus_vectors.cpp` (genesis, addresses, coin-id, WOTS pub/sig)
- the Totem Java interop KATs (when the MASH/MSHS2 tree is present)

The primitives benchmarked here (Winternitz/WOTS via BouncyCastle GMSS, SHA3-256,
MMR) are algorithmically shared, so a *performance* comparison is meaningful even
though byte parity is not claimed.

## What is measured

| Metric | Java (`BenchMinima`) | C++ |
|---|---|---|
| `wots_pubkey_ms` | `new Winternitz(seed).getPublicKey()` | `Winternitz(seed).getPublicKey()` |
| `wots_sign_ms` | `w.sign(msg)` | `w.sign(msg)` (batched+pooled if backend installed) |
| `wots_verify_ms` | `Winternitz.verify(...)` | `Winternitz::verify(...)` |
| `sha3_224_hashes_per_sec` | `Crypto.hashData(224B)` loop | `mining::sha3Into(224B)` loop (single-thread) |
| `mmr_add_entries_per_sec` | `MMR.addEntry` × N + `getRoot` | `MMR::addEntry` × N + `getRoot` |

Java `wots_*` is single-threaded; C++ `wots_sign` reports both scalar and the
batched+pooled path (x4 SIMD + bounded worker pool).

## Layout

```
reference-bench/
  README.md                 this file
  java/BenchMinima.java     Java harness (against the minima-core clone)
  java/BenchRam.java        Java MMR RAM-scaling harness
  java/BenchMempool.java    Java mempool RAM-scaling + coin-lookup harness
  cpp/bench_sha3.cpp        C++ SHA3-256 224B hashrate bench
  cpp/bench_ram.cpp         C++ MMR RAM-scaling harness
  cpp/bench_mempool.cpp     C++ mempool RAM-scaling + coin-lookup harness
  cpp/bench_sqlbatch.cpp    C++ TxPoWSqlDB write throughput (batched vs per-row)
  scripts/build_minima_core.sh      clone + compile minima-core + harness (offline, javac)
  scripts/run_all.sh                env capture + microbenches -> results/<timestamp>/
  scripts/run_node_bench.sh         full-node comparison (solo fresh chain)
  scripts/run_ram_bench.sh          component RAM scaling (MMR)
  scripts/run_mempool_bench.sh      component RAM scaling + lookup (mempool/RamDB)
  scripts/run_sql_bench.sh          SQLite write throughput (batched vs per-row)
  scripts/run_node_ram.sh           full-node RSS soak (steady-state / leak)
  scripts/run_node_throughput.sh    online transaction throughput (RPC-driven)
  scripts/run_full_sync.sh          initial sync time + fully-synced RAM
  scripts/run_cpu_idle.sh           CPU at idle (CPU-seconds / hour)
  scripts/run_tx_load.sh            CPU / latency under transaction load
  scripts/run_disk_io.sh            disk writes per hour/day (incl. WAL)
  scripts/_node.sh                  shared node-launch / rss / cpu / io helpers
  scripts/run_sweep.sh              one-command full sweep -> SWEEP_SUMMARY.md
  results/<timestamp>/              environment.txt + per-run raw outputs + summaries
```

## Benchmark kinds

1. **Microbenchmarks** (`run_all.sh`) — WOTS pubkey/sign/verify, SHA3-256, MMR.
   `cpp/bench_sqlbatch.cpp` (CMake target `bench_sql_batch`) also measures TxPoWSqlDB
   write throughput, batched vs one-transaction-per-row.
2. **Full-node comparison** (`run_node_bench.sh`) — runs each node on a **fresh solo
   private chain** (`-solo -daemon -clean`), which forces the deterministic heavy
   startup path (seed → initial keygen → genesis-block creation → daemon bring-up),
   and measures: genesis/ready time, **peak & idle RSS**, **thread count**, data-dir
   size. This is the device-resource headline.
3. **RAM harness** — `run_ram_bench.sh` scales RSS with N MMR entries (bytes/entry);
   `run_mempool_bench.sh` scales mempool RSS with N transactions / input coins into
   a `RamDB` and times `checkForCoinID` (this node's O(1) coin index vs the Java
   reference's linear scan);
   `run_node_ram.sh` soaks a running node and reports RSS min/avg/max/last + slope
   (steady-state / leak indicator).
4. **Online throughput** (`run_node_throughput.sh`) — drives a solo node with real
   transactions over RPC (`send`), since TxPoW work happens on a transaction. It
   measures cold-boot → first-spendable time and per-transaction send latency
   (sign + TxPoW mine + mempool/cascade/MMR). RPC auth uses user `minima` + the
   `-rpcpassword`; params are URL-encoded space-separated (`send%20address:..%20amount:1`).

   For a fair comparison the C++ node must be an **optimised** build:
   ```sh
   cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/homebrew
   cmake --build build-bench --target minima -j 8
   CPP_BIN=$PWD/build-bench/minima reference-bench/scripts/run_node_bench.sh
   ```
   (Bootstrapping the process-wide WOTS backend at startup means the node exercises
   the optimised signing path during genesis keygen.)

## Reproduce

```sh
reference-bench/scripts/run_all.sh
```

Prerequisites: a JDK (defaults to `/opt/homebrew/opt/openjdk@17`), `c++` (clang),
OpenSSL + Boost headers in `/opt/homebrew/include`. The `minima-core` clone is
expected at `../reference/minima-core` relative to the CRYSTAL_LABS root (or set
`MINIMA_CORE=`).

### Why a manual `javac` build?

`minima-core`'s `build.gradle` resolves dependencies from the now-defunct
`jcenter()`. All needed jars are vendored in its `lib/`, so we compile the source
tree directly with `javac` against `lib/*.jar`, sidestepping Gradle/network.
It needs Java 10+ (uses `BigDecimal.sqrt`); we build/run with JDK 17.

## Remote lab (OCI Ampere A1)

The reference repo (`totem-node-new-crypto`) carries the connection details for an
existing OCI Ampere A1 research instance (`tests/mshs2/bench/remote/oci/`: `oci.env`,
`lib.sh`, `run.sh`; `oci` CLI + `~/.ssh/mash_oci_ed25519`). We reuse that lab for
device-representative runs: upload this source, build Release on the A1, run the
benchmarks, and pull artifacts back into `results/`.

Operational notes learned the hard way:
- Localhost P2P needs `-allowallip`, else the node refuses `127.0.0.1` peers.
- The harness scripts must be `set -e`-portable: `cmd=$(...)` returning non-zero
  and `[ … ] && break` at statement level abort under bash 5 (Linux) though not
  bash 3.2 (macOS). Fixed with `|| true` / `if … then … fi`.
- `pkill -f <pattern>` where the pattern appears in the pkill command line kills
  the SSH shell itself — use `pkill -x <procname>` on the remote.
- Detach long remote runs explicitly (`setsid … </dev/null &`); backgrounded nodes
  otherwise hold the SSH channel open.

See `RESULTS_LOG.md` for the canonical, chronological record. The current
one-host AWS 8 GB (Neoverse-N1) v0.2 results are in the 2026-10-20 entry, with raw
output under `results/aws8-*`.

## Result recording convention

- The **canonical, chronological log is `RESULTS_LOG.md`**; every campaign gets a
  dated entry pointing at its `results/<dir>/`.
- Every run writes a timestamped directory under `results/` containing the
  captured environment/provenance, raw tool output, and a `SUMMARY.md` table.
- Keep raw output verbatim; put interpretation in `SUMMARY.md` / this README.
- Record the `minima-core` commit hash with every result (it is in
  `environment.txt`).
