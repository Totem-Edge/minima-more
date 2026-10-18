# Optimization Backlog

Running record of performance/resource work ported from the Totem reference
(`totem-node-new-crypto`) and the remaining opportunities found by a node-wide
survey. Focus: **device resources** — CPU, RAM, disk I/O, threads, network.

Legend: **DONE** = applied & verified in this tree · **OPEN** = not yet done ·
**N/A** = reference item that does not apply to this fork.

Reference performance benchmarks against the Java node (`spartacusrex-minima/minima-core`)
live in `reference-bench/` — see `reference-bench/README.md` (methodology + the
"performance only, not byte-parity" caveat) and `reference-bench/RESULTS_LOG.md`.

---

## Already applied

| Area | Change | Ref |
|---|---|---|
| Consensus | `checkParents` `getSuperParent(i)`; `CURRENT_NETWORK` literal init | `03f7e90`,`f79788e` |
| Consensus | WOTS/Java BC-GMSS seed expansion | `2f6738b` |
| Consensus | `TxPoW::deepCopy` member-copy (no `const_cast`); explicit-length string | `c504cb9` |
| Consensus | null-token guards + remove `const_cast` in checker | `fcd34df` |
| Consensus | MiniData wire cap 512 MB → 16 MB | `c504cb9` |
| Consensus | greeting chain-id advertise + reject mismatched peers | `4a56e03` |
| Consensus | golden-vector test (`test_consensus_vectors`) | `65d820b` |
| Signature | `Crypto::sha3Md/sha256Md` (one `EVP_MD_fetch`, no per-call locked fetch) | `2f9dbe4` |
| Signature | RAII thread-local EVP contexts (no per-thread leak) | `3e97cbd` |
| Signature | WOTS x4 SIMD + bounded pool backend (`mash_wots_backend.hpp`) | `45c6bb4`,`78ac98b` |
| Signature | batched `sign()`/`verify()` (lockstep x4) + cached `mPrivateChains` | `ac3711a` (+ our caching) |
| TXPOW | bounded parallel nonce mining (`mshs2_mining.hpp`, `mineParallel`) | `ee29ca2`,`49ae653`,`a70716f` |
| MMR | MegaMMR root pointer cache in `getEntry()` | `cd91fd1` |
| MMR | `txnmmr` MMR-proof genesis fallback | `4a56e03` |
| DB | RamDB mempool coin index (`mCoinToTxPoW`) — O(1) `checkForCoinID` | `3d2593a` |
| DB | Cascade thread-safety (`shared_mutex` + `shared_ptr` + `getParent`) | `d3ebbea`,`7d24632` |
| Cleanup | Maxima/MDS surface removed (NIO, params, userprefs, command list) | — |

N/A (no MASH/MSHS2 tree in this fork): raw forest, PK_ROOT dedup, parallel
work-stealing forest, x4 forest batching, depth-15 re-base.

---

## OPEN — CPU

### 1. Crypto serialization allocates 3× per hash  — DONE
- **DONE:** `hashObject/hashObjects/hashAllObjects` now serialize via a custom
  `streambuf` into a thread-local buffer and hash in place (no `oss.str()` /
  vector copies). MMR build 3282→2959 ms (−10%; 3785→2959, −22% total) on M1;
  byte-identical (consensus 12/12). Benefits every hash in the node.
- **Where:** `org/minima/utils/crypto.cpp:182-230` (`hashObject`, `hashObjects`, `hashAllObjects`): `std::ostringstream` → `oss.str()` (copy) → `std::vector<uint8_t>` (copy).
- **Why it matters:** this is the hottest primitive in the node (every MMR node, TxPoW ID, Merkle combine, MMR proof).
- **Fix:** a `std::streambuf` writing straight into a reused thread-local buffer, or serialize directly into the digest sink. Removes 2 of 3 allocations.

### 2. `MinimaLogger::log` does real work on every call  — HIGH
- **Where:** `org/minima/utils/minima_logger.cpp:54-72`; `getProcessMemoryBytes()` at `:120` **opens/reads `/proc/self/statm`** (Linux) or does a Mach `task_info` syscall (macOS); `:70` `std::cout << … << std::endl` **flushes**.
- **Why it matters:** ~hundreds of call sites, many unconditional `"INFO : …"` (NIO 34, message 43, processor 33, checker 92). Each = syscall + formatting + flushed write (also flash wear on SD-card devices).
- **Fix:** level gating, cache/lazy the memory read, drop the per-line flush, and demote unconditional hot-path logs.

### 3. `MMR::getPeaks()` returns by value  — MEDIUM
- **Where:** copied in ~10 hot paths, e.g. `org/minima/objects/mmr/m_m_r.cpp:97,159,337,429,476,552,661,702`.
- **Fix:** use `getPeaksRef()` for read-only callers (mind the invalidation that triggered the `7d24632` revert), or return a `const&` view.

### 4. `MiniNumber` arbitrary-precision (`boost::cpp_int`)  — MEDIUM
- **Where:** amounts/block numbers compared and printed via `cpp_int`; weight math uses `cpp_dec_float_100` (`tx_po_w.cpp` `calculateTXPOWID`).
- **Fix:** fast path for small values (fit-in-uint64) on compare/add/toString.

---

## OPEN — RAM

### 5. `RamDB::getCompleteMemPool()` returns a full copy  — LOW
- **Where:** `org/minima/database/txpowdb/ram/ram_d_b.cpp:104`; only caller `.../commands/base/mempool.cpp:41` (RPC).
- **Fix:** return `const&` / a view.

### 6. SQLite cache/mmap tuning is fixed, not device-aware  — MEDIUM (constrained)
- **Where:** `org/minima/utils/sql_d_b.cpp:82` `cache_size=-16384` (16 MB), `:86` `mmap_size=256 MB`.
- **Fix:** derive from available RAM / make configurable.

### 7. `MiniData` payload copies  — MEDIUM
- 32–64-byte `std::vector` heap payloads copied pervasively; consider SBO / `std::array`-backed small representation.

---

## OPEN — Disk I/O

### 8. SQLite statement reuse / write batching  — HIGH (during sync/IBD)
- **Where:** `txpowdb/sql` re-prepares statements; block/tx writes not wrapped in explicit transactions.
- **Fix:** cache prepared statements; wrap IBD/archive writes in `BEGIN…COMMIT`; `PRAGMA synchronous=OFF` for bulk sync only.

### 9. WAL checkpoint sizing  — MEDIUM
- **Where:** `org/minima/utils/sql_d_b.cpp:222` `wal_checkpoint(TRUNCATE)`.
- **Fix:** checkpoint by WAL size rather than fixed schedule (flash churn).

### 10. stdout log flush (see #2) is also disk wear  — HIGH (SD-card)

---

## OPEN — Threads

### 11. Pools are hardcoded / not sized to cores  — HIGH (small devices)
- NIO thread pool = **4** (`n_i_o_manager.cpp:84,215`).
- WOTS pool = `min(2·hw, 8)` (`mash_wots_backend.hpp:273-292`).
- Mining pool = up to `hw` (`mshs2_mining.hpp:113`).
- ~18 long-lived spawn sites (one per `MessageProcessor`).
- **Fix:** derive all pool sizes from `hardware_concurrency()` with a low cap, expose via config.

---

## OPEN — Network

### 12. Per-message allocation + parsing overhead  — MEDIUM
- Each incoming frame builds a `shared_ptr<NIOMessage>` + `MiniData` copy (`n_i_o_manager.cpp`, `n_i_o_message.cpp`); dominates background CPU under many peers.

### 13. Networking hardening still open (also resource-relevant)  — HIGH
From the original review §2, not yet ported:
- `NIOClient::MAX_MESSAGE` 256 MB → 16 MB (`d3ebbea`).
- NIOServer connection cap / HTTP connection limits + thread-pool reuse (`d3ebbea`).
- IBD queue depth cap (10k) (`d3ebbea`).
- per-peer rate limiter + sync-critical exemption (`08f3128`).
- webhook SSRF guard (`4a56e03`).
- `connection_backpressure` / `peer_reputation` subsystems (`95c2061`).

---

### 0. MMR / UTXO-set framing  — HIGHEST memory priority (A1-confirmed)
- **Measured (OCI Ampere A1, g++ -O3):** ~860 B/entry RSS, strictly linear —
  100k=86 MB, 500k=420 MB, **1M=838 MB**. See `reference-bench/RESULTS_LOG.md`
  (2026-10-17) and `reference-bench/results/20261017-oci-a1/`.
- **Diagnosis:** each MMR entry is a chain of ~5–6 small heap allocations
  (`MMREntry`→`unique_ptr<MMRData>`→`unique_ptr<MiniData>`+`unique_ptr<MiniNumber>`,
  plus `unique_ptr<MMREntryNumber>`), and an MMR stores ~2 entries per leaf. The
  allocator overhead per tiny object, not the 32-byte payload, dominates.
- **Fix (byte-identical, serialization unchanged):** inline the `unique_ptr`s to
  by-value members in `MMRData` and `MMREntry`; key the entry maps compactly; give
  `MiniData` a fixed 32-byte inline buffer (`MiniNumber` too). Removes ~4 allocs
  per entry.
- **DONE (steps 1+2):** `MMRData`'s and `MMREntry`'s `unique_ptr` members are now
  by-value (empty-entry sentinel preserved via `mIsEmpty`) → **858 → 762 B/entry
  (−11.2%)** on the A1; serialization byte-identical; tests + node smoke pass.
- **Step 3 (map value by-value):** memory-neutral (762→762); kept for fewer allocs.
- **Step 4 (MiniData SBO) DONE:** new `MiniBuffer` (inline 32 B) with an implicit
  `std::vector` conversion; `MiniData::mData` migrated; `getBytes()` now returns
  `const MiniBuffer&` (serialization unchanged) → **762 → 698 B/entry (−8.4%)**,
  cumulative **858 → 698 B/entry (−18.6%)**. Tests + node smoke pass.
  See `MINIDATA_SBO_MIGRATION_SCOPE.md`.
- **MMR CPU (DONE):** profiled; replaced the string entry-map key with a packed
  `uint64_t` (`row<<56 | entry`) + `MMREntryNumber::toUint64()`. MMR build
  **3785 → 3282 ms (−13%)** on M1 (C++/Java gap 1.61×→1.40×), memory ~698→634
  B/entry on A1. Remaining MMR-build cost is hashing (crypto serialization,
  backlog #1) + `MiniNumber` BigDecimal precision (consensus-sensitive).
- **Next (optional):** `MiniNumber` int64 fast path (cpp_int measured NOT
  dominant — low ROI).
- **Entry-count reduction — analysed** (`MMR_ENTRY_COUNT_ANALYSIS.md`): cannot be
  done safely in the consensus MMR (pruning changes the root → consensus break;
  it is valid only for the auxiliary MegaMMR). Real levers are deploy-time:
  **MegaMMR offload/prune modes** (`-megammr`, `-megaprune*`) for RAM-constrained
  devices, and the retained-window parameter (`MINIMA_CASCADE_START_DEPTH=2048`
  vs proof history 256) — a network/consensus parameter, not a local change.

## Suggested order (biggest device wins first)

1. **#2 cheap logger** — CPU + flash wear, self-contained, low risk.
2. **#1 crypto serialization** — deepest validation hotspot.
3. **#11 pool sizing from cores** — small-device CPU/RAM.
4. **#8 SQLite batching + statement reuse** — sync-time disk.
5. **#13 networking hardening** — DoS resilience / resource caps.
6. Then #3, #4, #6, #7, #5.

---

## Verification harness in place
- `test_consensus_vectors` (12/12), `test_winternitz` (5/5), `test_mmr`, `test_mempool_index` (20/20), `bench_wots_signature`, and the standalone mining-equivalence check.
- `minima` builds clean under `-Wall -Wextra -Wpedantic`.
