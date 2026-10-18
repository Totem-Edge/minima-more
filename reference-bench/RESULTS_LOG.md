# Reference benchmark results log

Chronological record of runs of `reference-bench/scripts/run_all.sh`
(Java `minima-core` vs this C++ node). Each run's raw output lives in
`results/<timestamp>-<arch>/`.

> **Noise caveat.** These are wall-clock microbenchmarks on a shared machine and
> vary run-to-run (observed: C++ batched WOTS sign 0.89–1.83 ms/op across runs).
> Treat single runs as indicative, not decision-grade. For publishable numbers,
> run on a quiet host with several repetitions — the same discipline the upstream
> project uses for its OCI/A1 measurements. Record the host load if known.

> **Scope.** `minima-core` is a **performance reference only**, not a byte-parity
> oracle (it may use a different address-derivation path). Correctness is anchored
> to `tests/test_consensus_vectors.cpp` and the Totem interop KATs.

## How to read a run

- `environment.txt` — host, CPU, cores, RAM, compiler, JDK, minima-core commit.
- `java_bench.txt` — Java `minima-core` metrics (`metric<TAB>value<TAB>unit`).
- `cpp_sha3.txt`, `cpp_wots_signature.txt` — C++ metrics.
- `SUMMARY.md` — the side-by-side table.

---

## 2026-10-16 — Apple M1 (8 cores), 16 GB, macOS 25.2, OpenJDK 17.0.16
minima-core `c2a54a2` · run `results/20261016-162107-arm64` · **uncommitted C++ working tree**

| Metric | Java (minima-core) | C++ (this node) | C++ advantage |
|---|---|---|---|
| WOTS pubkey (ms/op) | 5.30 | 1.11 | ~4.8× |
| WOTS sign (ms/op) | 2.88 | 0.63 (batched+pooled) | ~4.6× |
| WOTS verify (ms/op) | 2.21 | 0.58 | ~3.8× |
| SHA3-256 224 B (h/s, 1 thread) | 569,428 | 2,483,883 | ~4.4× |
| MMR addEntry+getRoot (entries/s) | 171,264 | 290,017 | ~1.7× |

C++ WOTS sign, scalar vs our batched path (same run): scalar 2.78 ms → batched
1.31 ms = **2.13×**, output identical.

Notes:
- **High run-to-run variance** on this shared M1 (e.g. C++ SHA3 ranged
  1.46M–2.48M h/s across three consecutive runs; Java sign 2.24–2.88 ms). Treat
  as indicative; decision-grade numbers need a quiet host + multiple reps.
- The SHA3 row is **single-threaded** on both sides — it is the per-core mining
  inner loop, not our multi-core `mineParallel` throughput (which spans cores).
- The C++ WOTS figures include the process-wide installed backend (x4 SIMD +
  bounded pool); Java is single-threaded BC. That is the intended comparison
  (optimised port vs reference implementation), not like-for-like threading.

## 2026-10-16 — Full-node comparison (solo, fresh private chain)
Apple M1 (8 cores), 16 GB, macOS 25.2 · OpenJDK 17.0.16 · minima-core `c2a54a2`
· run `results/20261016-164425-arm64` · C++ = **optimised** `build-bench/minima`

| Metric | C++ (this node) | Java (minima-core) | C++ advantage |
|---|---|---|---|
| genesis creation (s) | 1.99 | 3.19 | ~1.6× faster |
| daemon ready (s) | 2.00 | 3.20 | ~1.6× faster |
| peak RSS (MB) | 14.3 | 271.8 | **~19× less** |
| idle RSS (MB) | 14.3 | 271.8 | **~19× less** |
| threads | 23 | 59 | ~2.6× fewer |
| fresh data dir (kB) | 504 | 184 | (C++ larger) |

A first pass had used the Debug (`-O0`) C++ binary and showed C++ *slower* at genesis
(6.0 s vs 2.5 s) — a reminder to always compare optimised builds. The Release node
flips that (2.0 s vs 3.2 s) and keeps the ~19× RAM and ~2.6× thread advantage.

Notes:
- Java's RSS is the JVM process footprint (heap/metaspace/code cache); it stays high
  at idle. On a constrained device this is the decisive number.
- Single run each. Startup times vary (~±1 s); re-run on a quiet host for
  decision-grade figures.
- **Not yet covered:** online sync/block-processing throughput (needs a scripted
  multi-block private chain for both nodes). RSS/threads/startup are the robust
  full-node metrics; throughput is future work.
- The larger C++ data dir likely includes the SSL keystore + driver/DB files; worth
  a look if flash footprint matters.

## 2026-10-16 — Component RAM scaling (MMR, N=100k entries)
M1 · run `results/20261016-230120-arm64`

| Metric | C++ (RSS) | Java (heap-in-use) |
|---|---|---|
| bytes / MMR entry | ~737 | ~29 (noisy, GC-dependent) |
| delta for 100k entries | ~72 MB | ~2.8 MB (heap live-set) |
| process peak RSS | ~74 MB | ~206 MB (whole JVM) |

Caveats: C++ is process RSS, Java rows are JVM heap-in-use — **different units**.
The Java heap figure is unreliable under this method (GC sawtooth); its RSS is
JVM-baseline-dominated. The C++ ~737 B/entry (leaf+internal nodes) is the useful
number and a candidate for the backlog (MMR RAM per entry).

## 2026-10-16 — Full-node RSS soak (solo, idle)
`run_node_ram.sh` · Java samples 30
- Java: min 74 MB, avg 116 MB, **max 264 MB**, sawtooth (GC), negative slope run-to-run.
- C++: min 10.4 MB, avg 12.0 MB, **max 15.2 MB** (RSS settles after genesis; the
  negative slope is the startup peak decaying, not a leak).
Without a scripted workload this is a steady-state/baseline-leak profile; JVM GC
makes Java's series non-monotonic.

## 2026-10-16 — Online transaction throughput (solo, RPC `send`)
runs `results/20261016-232607-arm64` (C++), `...-232930-arm64` (Java)

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| cold-boot → first spendable (s) | 82.6 | 63.7 |
| sends ok / failed | 3 / 0 | 1 / 2 |
| per-tx send latency | 337–397 ms | ~1144 ms |
| blocks added | 6 | 1 |
| peak RSS (kB) | 15,360 | 263,808 |
| threads | 24 | 63 |

A send = sign + TxPoW mine + mempool/cascade/MMR. C++ per-tx latency is ~3× lower.
Caveats: each send spends the only confirmed coin, so subsequent sends wait for
change to confirm at the solo automine cadence (~40 s); Java's send-success rate
was lower here (wallet confirmation behaviour), so treat the latency as the
meaningful comparison, not the raw tx/s. Single run.

## 2026-10-17 — Two-node P2P sync + relay (C++ miner -> C++ follower)
run `results/20261016-234951-arm64` · `run_sync_bench.sh` (both nodes `-allowallip`)

| Metric | Value |
|---|---|
| miner chain | 6 blocks (genesis + ~5 automined) |
| **IBD latency** (follower connect -> tip matched) | **0.71 s** |
| IBD payload | 3.6 KB / 6 blocks |
| **live relay latency** (miner mines -> follower sees) | **39 ms** |
| miner RSS | 13.2 MB |
| follower RSS after sync / peak | 16.8 MB / 14.7 MB |

The follower IBDs instantly and tracks live blocks within ~39 ms. Chain is small
because solo/test automine yields ~1 block / 40–50 s; a large-chain throughput run
needs a big `MINE_SECONDS`. Cross-impl (miner C++ → follower Java and vice-versa)
is supported by the script (`MINER_TYPE`/`FOLLOWER_TYPE`) — relevant to the P2P
rate-limiter fix — but the Java automine cadence is slow, so a Java run needs a
longer window (relay waits are now capped).

Note: two-node sync required `-allowallip` — the node blocks localhost P2P by
default (`p2_p_functions.cpp` `isIPLocal` + `ALLOW_ALL_IP`).

## 2026-10-17 — OCI Ampere A1 (Neoverse-N1) confirmation
Remote lab instance `mash-a1-research` (4 cores / 24 GB, aarch64, g++ -O3).
Artifacts: `results/20261017-oci-a1/` (MMR curve, node metrics, sync logs).

### MMR RAM curve — confirms the "MMR framing dominates" hypothesis
Adding N MMR entries to a fresh MMR, RSS delta:

| N entries | bytes/entry | RSS delta |
|---|---|---|
| 100,000 | 883 | 86 MB |
| 500,000 | 860 | 420 MB |
| 1,000,000 | **858** | **838 MB** |

**Strictly linear at ~860 B/entry across a 10× range** — so the MMR framing is
confirmed as a dominant, unbounded RAM term: ~860 MB per 1M entries/coins. On the
24 GB A1 that's fine; on a 1–2 GB device it is the deciding constraint.

### Node + sync on the A1 (C++)
| Metric | Value |
|---|---|
| node ready (solo fresh) | 3.48 s |
| node peak / idle RSS | 17.9 MB / 18.6 MB |
| node threads | 24 |
| two-node IBD (3 blocks) | 3.6 s |
| live relay latency | 26 ms |

The node's *baseline* is ~18 MB; the MMR is what pushes it toward hundreds of MB
as the UTXO set grows — exactly the framing cost the microbenchmark predicted.

Caveat: the curve is a fresh, unchained grow (no parent-chaining/pruning), so it
bounds the per-*distinct-entry* cost of the current framing; the live node spreads
entries across parent-chained per-block MMRs. Either way it validates the order of
magnitude and the linearity, and the optimisation direction (packed/arena MMR
storage, fixed-size MiniData) stands.

## 2026-10-17 — MMR framing optimisation, step 1 (byte-identical)
Inlined `MMRData`'s two `unique_ptr` members (`MiniData`, `MiniNumber`) to
by-value members. Serialization is unchanged; `test_mmr` and
`test_consensus_vectors` pass; the full `minima` node builds.

`MMREntry`'s two `unique_ptr` members (`MMREntryNumber`, `MMRData`) are also
now by-value; the null-sentinel for "empty" entries is preserved via `mIsEmpty`
(`getMMRData()` returns nullptr when empty). `MMRData`'s default ctor was made
public. `test_mmr` + `test_consensus_vectors` pass; node builds and creates
genesis/coins (smoke).

A1 MMR curve, baseline → step 1 (MMRData by-value) → step 2 (MMREntry by-value):

| N | baseline | step 1 | step 2 |
|---|---|---|---|
| 100,000 | 883 | 819 | 787 |
| 500,000 | 860 | 796 | 764 |
| 1,000,000 | 858 | 794 | **762** |

Cumulative **858 → 762 B/entry (−11.2%)**, byte-identical (serialization
unchanged). Removes ~4 of the ~6 small heap allocations per entry.

### Step 3 (attempted): MMREntry map value by-value — memory-NEUTRAL
`mSetEntries` `unordered_map<string, unique_ptr<MMREntry>>` → `unordered_map<string, MMREntry>`
(one fewer allocation per insert). Tests + node build/smoke pass, but A1 RSS is
unchanged (762.0 → 762.0 B/entry): the map node grew by the inline `MMREntry`
while the separate allocation disappeared — net zero. Kept (fewer allocations,
no behaviour change) but not a RAM win.

### Step 4 — MiniData small-buffer (SBO) migration — DONE
Implemented as scoped in `MINIDATA_SBO_MIGRATION_SCOPE.md` (option B):
- New `MiniBuffer` (inline 32 B, heap fallback above) with a vector-like read
  API + an implicit `operator std::vector<uint8_t>()`, so existing call sites
  compile with ~zero churn.
- `MiniData::mData` → `MiniBuffer`; `getBytes()` now returns `const MiniBuffer&`;
  `BaseConverter::encode16` gained a `(ptr,len)` overload to avoid copies in
  `to0xString`; internal helpers (`readFully`/`stripLeadingZeros`/
  `compareMagnitude`) switched to `MiniBuffer`.
- One real bug found by the tests: a `union`-based inline/heap discriminator read
  payload bytes as the heap pointer → SIGSEGV. Fixed with a separate heap
  pointer. (Tests caught it immediately: `test_mmr` SIGABRT / consensus SIGSEGV.)

Gates: `test_mmr`, `test_consensus_vectors` (12/12), `test_winternitz` (5/5),
`test_mempool_index` (20/20), full `minima` build, and a genesis/balance smoke
test all pass. Serialization unchanged.

A1 MMR curve, steps 1–2 → step 4 (bytes/entry):

| N | steps 1–2 | + MiniData SBO |
|---|---|---|
| 100,000 | 787 | **723** |
| 500,000 | 764 | **700** |
| 1,000,000 | 762 | **698** |

**762 → 698 B/entry (−64 B, −8.4%)** consistently across scales. Combined with
the baseline, **858 → 698 B/entry (−18.6%)**, byte-identical.

`MiniNumber`'s `cpp_int` was measured NOT dominant (~62 B ≈ inline `sizeof`), so
its risky arithmetic fast-path was correctly not attempted.

## 2026-10-18 — MMR efficiency: this node vs minima-core (framework run)
`run_ram_bench.sh N=1000000` · run `results/20261018-061608-arm64`
(reference = spartacusrex-minima/minima-core `c2a54a2`, built from the clone)

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| bytes / MMR leaf (C++ RSS vs Java heap) | 698.3 | 601.9 (heap) |
| memory delta for 1M leaves | 666 MB (RSS) | 574 MB (heap) |
| **process peak RSS** | **668 MB** | **1891 MB** |
| MMR build time (1M leaves) | 3785 ms | 2270 ms |

Reading it honestly:
- **Device memory (process RSS): C++ is ~2.8× lighter** — 668 MB vs 1.89 GB for
  the same 1M-entry MMR. This is the number that matters on constrained devices;
  the JVM's overhead/committed heap dominates the Java figure.
- **Per-entry heap live-set:** Java's ~602 B (heap) is comparable to C++'s ~698 B
  (process RSS) — different units, so not directly comparable, but it shows the
  C++ framing is now in the same ballpark rather than the pre-optimisation
  ~858 B.
- **Build CPU:** Java is ~1.7× faster to build the MMR (2.27 s vs 3.79 s). A
  real CPU datapoint for the C++ MMR (string-keyed entry map + `cpp_int`-based
  entry numbers are candidate costs) — memory was the goal here, but this is
  worth a separate CPU pass.

Caveat: single run; C++ RSS vs Java heap are different measures (peak RSS is the
only like-for-like row). Run on the A1 for device-representative absolute numbers
(relative result expected to hold).

## 2026-10-18 — MMR CPU: compact entry-map key (profiled fix)
`perf` on the A1 showed the MMR build dominated by the **string-keyed entry map**
(`getHashTableEntry` + `unordered_map<string,...>` ≈ 28%) and
**`MMREntryNumber::toString()` decimal conversion** (~16%), plus hashing (~19%)
and `MiniNumber` BigDecimal precision math (~20%).

Fix: `MMREntryNumber::toUint64()` + key `(row << 56) | entryNumber` and
`mSetEntries` → `unordered_map<uint64_t, MMREntry>`. Serialization unchanged.

| Metric | before | after |
|---|---|---|
| MMR build, 1M leaves (M1) | 3785 ms | **3282 ms (−13%)** |
| Minima-core Java build (M1) | 2345 ms | 2345 ms |
| C++/Java build gap | 1.61× | **1.40×** |
| bytes/entry (A1) | 698 | **634 (−9%)** |

Tests (`test_mmr`, consensus 12/12, winternitz 5/5, mempool 20/20), full node
build and genesis smoke all pass.

Remaining MMR-build cost (from the profile): `Crypto::hashAllObjects` hashing
(~19–28%, the ostringstream serialization path — backlog #1) and `MiniNumber`
BigDecimal precision in `CreateMMRDataParentNode` (~20%, consensus-sensitive).
The next safe CPU win is the shared **crypto-serialization** optimization.

## 2026-10-18 — Crypto serialization: hash in place (shared hot path)
`Crypto::hashObject/hashObjects/hashAllObjects` previously built a
`std::ostringstream`, copied to a `std::string` (`oss.str()`), then copied again
into a `std::vector<uint8_t>` before hashing. Replaced with a small custom
`std::streambuf` (`ByteStreamBuf`) that captures the serialized bytes directly
into a reusable **thread-local** buffer, then hashes in place — no string/vector
copies. Byte-identical (`test_consensus_vectors` 12/12); hits every hash in the
node (MMR parents, TxPoW IDs, Merkle, signatures).

| Metric | before | after |
|---|---|---|
| C++ MMR build, 1M leaves (M1) | 3282 ms | **2959 ms (−10%)** |
| vs original baseline (M1) | 3785 ms | **2959 ms (−22%)** |
| C++/Java build gap (M1) | 1.40× | **~1.19–1.26×** |
| A1 build (1M) | 5235 ms | 5024 ms (−4%, noisy) |
| bytes/entry | unchanged | ~634 (A1) / ~689 (M1) |

All gates pass: consensus 12/12, `test_mmr`, winternitz 5/5, mempool 20/20,
full node build, genesis smoke.

Remaining MMR-build cost: `MiniNumber` BigDecimal precision in
`CreateMMRDataParentNode` (~20%, consensus-sensitive) and `cpp_int` entry-number
arithmetic. The C++/Java build gap is now small.

## 2026-10-18 — MMR CPU part 2: MiniNumber + MMREntryNumber fast paths
Re-profile (A1) after the key + serialization fixes showed the remaining cost in
**`MiniNumber` precision** (`count_decimal_digits` converting the `cpp_int` to a
decimal string via `ostringstream`, called from `apply_precision_down` on every
construction/copy/add) and **`MMREntryNumber` cpp_int arithmetic**
(`modulo`/`increment`/`compare`).

Exact-preserving fast paths (all byte-identical; consensus 12/12):
- `MiniNumber::count_decimal_digits`: values fitting `uint64` (the common case —
  amounts/sums; MAX_DIGITS is 64) count digits with a `/10` loop, no decimal string.
- `MMREntryNumber::isEven()`: integer (scale 0) parity via the low bit — replaces
  the `modulo(TWO)` object construction `MMREntry::isLeft()` did per tree level.
- `MMREntryNumber::increment()` and `compareTo()`: scale-0 fast paths.

| MMR build, 1M leaves | M1 | A1 |
|---|---|---|
| before | 2940 ms | 4739 ms |
| after | **2216 ms** | **4016 ms** |
| Java (minima-core) | 2256 ms | — |

**Cumulative M1 C++ MMR build: 3785 → 2216 ms (−41%)**, and the C++/Java build
gap is now **closed (~0.98× — C++ slightly faster)**. A1: 4739 → 4016 ms (−15%;
A1 has a slower decimal/cpp_int path so the fast paths help more there).

All gates pass: consensus 12/12, `test_mmr`, winternitz 5/5, mempool 20/20, full
node build, genesis smoke. Memory unchanged (~634 B/entry A1 / ~689 M1).

## 2026-10-18 — Logger: cache the per-line memory syscall (backlog #2, partial)
`MinimaLogger::log()` called `getProcessMemoryBytes()` (a `/proc/self/statm` read,
or `task_info` on macOS) on **every** line, plus `std::endl` (flush) per line.
Now the resident-set figure is cached behind a **1s TTL** (`std::atomic`, lock-free;
`readProcessMemoryBytesUncached()` does the raw platform query). A ~1s-stale figure
is fine for a log line. **Flush is deliberately kept** (`std::endl`): the node is
normally run with stdout redirected to a file (fully buffered), and removing the
flush delayed lines such as "Genesis block created" - an observability regression.
Verified: node builds, genesis smoke logs 21 lines including "Genesis block created",
`/balance` = 1 coin. No consensus surface touched.

## 2026-10-18 — Mempool RAM + coin-lookup benchmark (new harness)
Added a mempool RAM-scaling harness: `cpp/bench_mempool.cpp`, `java/BenchMempool.java`,
`scripts/run_mempool_bench.sh`. It builds N transaction TxPoWs (each with `coinsPerTx`
input coins) into a `RamDB` and reports bytes/txpow, bytes/coin, build time, and
`checkForCoinID` lookup latency. The last is the point of interest: this node keeps an
O(1) `mCoinToTxPoW` index (at the cost of extra RAM); the Java reference scans the whole
pool linearly.

M1, N=100000 txns x 2 coins (single run, indicative):

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| bytes/txpow | 3434 (RSS) | 3678 (heap) |
| bytes/coin | 1717 (RSS) | 1839 (heap) |
| build (ms) | 2510 | 2576 |
| checkForCoinID (us/lookup) | **3.4** | **13631** |
| process peak RSS (kB) | 337376 | 1289136 |

The coin index costs no visible extra RSS vs the Java reference (3434 vs 3678 B/txpow)
while making the double-spend guard **~4000x faster** (3.4 us vs 13.6 ms per lookup).
Caveats: C++ is process RSS, Java is JVM heap-in-use (the peak-RSS row is the whole
JVM); heap != RSS; single run, indicative only.

## 2026-10-18 — SQLite batched writes + P2P pool sizing (backlog)
**TxPoWSqlDB batching.** TxPoW inserts arrive in bursts (sync / tree load /
restore), and each `addTxPoW` was its own SQLite transaction. Consecutive inserts
now share one transaction, committing every 512 rows (or 1s), so N autocommits
become ~N/512. Inserts in a batch are visible to the same connection (`exists()` /
`getTxPoW()` still work). The batch is committed on `flushBatch()`, the derived
`saveDB()`, `hardCloseSQLDB()`, `wipeDB()`, and connection close. `closeAndReopen()`
was restructured because it held `mLocalMutex` while `createSQL()` re-locks it - a
latent self-deadlock. A crash loses at most the last batch (TxPoWDB is rebuildable
from peers).

Measured (`bench_sql_batch`, M1, N=50000 distinct TxPoWs, DB write time only):

| | time | inserts/s |
|---|---|---|
| one txn per row (old) | 6262 ms | 7985 |
| batched (new) | 3142 ms | 15915 |

**~2.0x faster.** Correctness covered by `test_sql_batch` (9/9): exact count across
batch boundaries, rows read back, `INSERT OR IGNORE` still dedups, and data persists
across `closeAndReopen()`.

**P2P thread pool sizing.** `NIOManager`'s message pool was hardcoded to 4 threads.
It now scales with `hardware_concurrency`, clamped to [2, 8] (override
`MINIMA_NIO_WORKERS`), so bigger devices run more inbound messages concurrently while
RAM and DB contention stay bounded. Unchanged on <=4-core devices.

All gates pass: consensus 12/12, test_mmr, winternitz 5/5, mempool 20/20, sql_batch
9/9, node build, genesis smoke.

## 2026-10-18 — A1 full sweep (C++ vs Java, one command)
Ran `run_sweep.sh` on the OCI A1 (4-core aarch64, 24 GB) with the current source; the
Java reference (minima-core c2a54a2) was built on the same host, so this is a
same-hardware comparison. Raw output: `results/a1-full-20261018/`.

**MMR (1M entries)** — bytes/entry 634 (RSS) vs 602 (heap); build **4038 vs 3759 ms**
(~1.07x, C++ a touch slower on A1); process peak RSS **624 MB vs 1898 MB**.

**Mempool (200k txns x 2 coins)** — bytes/txpow 7575 (RSS) vs 3662 (heap); build
9207 vs 7663 ms; `checkForCoinID` **0.89 us vs 43733 us (~49,000x - O(1) index vs
linear scan)**; process peak RSS 1.48 GB vs 1.96 GB.

**SQLite write throughput** — 1519 -> 3222 inserts/s (**2.12x**, batched).

**Microbenchmarks (C++ vs Java)** — WOTS pubkey 1.90 vs 8.13 ms; WOTS sign (batched)
0.92 vs 4.30 ms; WOTS verify 1.03 vs 3.78 ms; SHA3-256 224B 1.16M vs 346k h/s; MMR
addEntry+getRoot 260k vs 139k entries/s. WOTS sign scalar -> batched 3.26 -> 1.07 ms
(3.05x).

**Full node (solo, fresh)** — genesis 5.91 vs 4.58 s (C++ slower here; on M1 C++ was
faster - single run, 4-core box); peak RSS **20 MB vs 302 MB**; idle RSS 18.7 vs
302 MB; threads **23 vs 52**; data dir 468 vs 212 kB. Node RSS soak: C++ ~18.7-19.8 MB,
slope ~40 kB/s (steady).

The whole comparison is one command: `scripts/run_sweep.sh` (component + node). Lab
additions this round: Linux-portable toolchain flags in `_env.sh`, `run_sql_bench.sh`,
`run_sweep.sh`, and portable thread counting (Linux `ps -o nlwp` vs macOS `ps -M`).

## 2026-10-18 — A1 full-lifecycle sweep (sync, CPU, disk)
Added the node-lifecycle benches (metrics 1-5) and ran them on OCI A1 (4-core ARM),
C++ vs Java on the same host. Raw output: `results/a1-lifecycle-20261018/`.

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| fully-synced RAM — peak during sync | 20.1 MB | 248.2 MB |
| fully-synced RAM — idle after sync | 21.7 MB | 177.6 MB |
| initial sync time (chain 11/15 blocks) | 5.89 s | 4.09 s |
| CPU at idle (CPU-seconds / hour) | 38.2 | 62.1 |
| CPU at idle (avg utilisation) | 1.06% | 1.73% |
| CPU under tx load (ms per tx) | 1635 | 2363 |
| send latency under load (ms avg) | 447 | 1965 |
| tx sends ok / failed | 4 / 0 | 3 / 1 |
| disk writes (B/hour) | 3.68 MB | 5.98 MB |
| disk writes (B/day) | 88 MB | 143 MB |
| threads | 25 | 58 |

New scripts: `run_full_sync.sh` (metrics 1+2), `run_cpu_idle.sh` (3),
`run_tx_load.sh` (4), `run_disk_io.sh` (5), plus shared `_node.sh` helpers.

**Caveats:** windows were reduced for the sweep (idle/disk 180 s; per-hour/day values
extrapolated); the sync chain is small (automine ~1 block / 40-50 s) so sync time
exercises the IBD mechanism, not bulk throughput, and the two runs synced slightly
different heights (the miner kept mining between them); single run on a shared host.

## Planned next
- Quiet-host, multi-rep harness mode for decision-grade numbers.
- Cross-impl / Java two-node sync (C++↔Java) and a **large-chain sync throughput**
  run (needs a long `MINE_SECONDS`, since automine is ~1 block / 40–50 s).
- Re-run the A1 sweep on a quiet host (the box was shared) with multiple reps.

(Done since the prior list: mempool RAM scaling -> `run_mempool_bench.sh`; both
backlog items - SQLite batched writes + thread-pool sizing - implemented and
benchmarked; one-command `run_sweep.sh` with Linux-portable toolchain flags.)
