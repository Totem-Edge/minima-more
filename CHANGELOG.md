# PureMinima C++ — Change Report

## v0.6.0 — IBD validation fix and graceful termination

This release fixes a block-validation bug on the IBD path. It is **not a
consensus rule change**: the rules a block must satisfy are unchanged, and no
valid block is affected. The node now applies those rules when inserting a block
received from a peer, instead of only on the live path.

Verified in both directions on this tree: a Java-mined chain, including a spend
block, is accepted by this node over real P2P/IBD; its own blocks are accepted by
the Java node; both sides report identical block IDs, and the kernel digest is
unchanged (`7ac470fb…b739ac` against oracle `c2a54a2`).

Upgrade together if you can: an unhardened node — earlier C++ releases, and the
Java implementation's IBD path — can still be tricked into accepting an invalid
block. Mixing hardened and unhardened nodes leaves that attack open on the
unhardened ones. Upgrading together removes the attack surface; it does not
introduce a new rule.

### IBD validation (present since v0.5.14)

- IBD insertion validates `checkTxBlockOnly`, block PoW, a previous-peaks match
  against the parent MMR, every input coin proof, duplicate inputs, the rebuilt
  MMR root/total against the committed header, and PoW for the first block on an
  empty tree.
- `TxPowTree` cumulative weight is no longer truncated and `setLength` returns
  main-chain ancestors only, both matching Java.

### Durability (new in 0.6.0)

- **SIGTERM/SIGINT now shut down gracefully.** Previously there was no handler:
  the default action killed the process mid-write, so nothing since the last
  periodic save reached disk. The handlers now run the same `Main::shutdown()`
  as the `quit` command — checkpoint, mempool flush, thread stop.
- The daemon and MCP loops treated a cleared `Main` instance as "not finished"
  and could never observe completion. They now treat a null instance as
  shutdown complete, so the process exits in ~2 s instead of needing SIGKILL.

This matters for `systemctl stop`, Kubernetes termination and any
`hawk-update`-style restart, all of which stop a node with SIGTERM.

### Changed since v0.5.14

- One greeting per connection: duplicate greetings are no longer answered with
  repeated IBD work.
- Cascade reconciliation uses the live heaviest tree branch first and falls back
  to the archive only for windows no longer in RAM.
- `memreport` reports file descriptors and thread counts.
- Opt-in IBD witness gate (`IBD_WITNESS_CHECK`, default **off**): validates
  transaction bodies that are already available locally and skips bodies that
  have not arrived, so acceptance stays deterministic across nodes.

### Fixed

- `loadBlockRange` returns `unique_ptr<TxBlock>`; callers now use `auto` instead
  of assigning to a `shared_ptr` vector (build break on current main).

### Known gaps

- IBD carries no transaction bodies, so witness/ownership signatures cannot be
  verified at insertion (coin proofs and the committed MMR are).
- Unsolicited IBDs are accepted without an outstanding sync request.
- The empty-tree root is PoW-gated but there is no trusted genesis/checkpoint.

### Compatibility

- Behaviour changes only for invalid blocks. Valid blocks, wire format, storage
  and archive files are unchanged. `MINIMA_VERSION` stays `1.0.100`.

## v0.5.14 — IBD consensus hardening, parity harness and a real test gate

### Consensus and correctness

- **D14: IBD insertion trusted the peer.** `processSyncBlock` now requires
  `checkTxBlockOnly`, a block-PoW (`isBlock`) gate, a previous-peaks match
  against the parent MMR, per-input coin-proof verification against that MMR
  (leaf nodes built one block ahead of the parent), duplicate-input rejection, a
  rebuilt MMR root/total match against the PoW-committed header, and a PoW gate
  for the first block on an empty tree. A crafted weight-0 next-height block
  previously became the chain tip on both C++ nodes *and* on the pinned Java
  oracle. Both C++ nodes now reject it: this is a
  **deliberate divergence from Java for invalid blocks only** — no valid
  block is affected, verified in both directions against the Java node.
- **Fork weight was truncated.** `TxPowTree::recalculateTree()` sorted nodes
  with an inverted comparator, so a parent summed its children's subtree
  weights before those children were finished. The sort is now descending by
  block number, so every child is complete before its parent is summed.
  Cumulative weight matches Java and is pinned by `test_txpowtree_weight`.
- **`setLength` kept fork blocks.** It appended non-ancestor blocks below the
  new root to the returned list, so they were treated as cascade candidates. It
  now returns exactly the new root's ancestors, matching Java; orphaned forks
  are dropped from the tree instead of migrated.
- **Graceful shutdown hung (D12).** `NIOManager::ThreadPool` joined workers
  unconditionally, so `saveAllDB()` never ran and a restart lost everything
  since the last periodic save. Stuck workers are detached after the drain
  deadline and shutdown proceeds.
- Test-only `MINIMA_TEST_PAUSE_FILE` mining pause in the generator, honoured
  only under `-test`, so fork-choice tests can freeze the tip without killing a
  node.

### Added — parity and test infrastructure

- **Level A (Java kernel differential):** `tests/parity/kernel` — C++
  `kernel_digest` plus `KernelOracle.java`, pinned to minima-core `c2a54a2`.
  Digest `7ac470fb…b739ac`, identical on minima-more, hawk-node and the oracle.
- **Level B (cross-node):** `replay_cross_node.py` (cpp2java, java2cpp, cpp2cpp,
  `--spend-every`), `reorg_cross_node.py`, `reject_corpus.py` (41 wire
  corruptions), `validate_corpus.py` (semantic checker cases), `java_node_smoke.py`.
- **Level D (restart):** `restart_equality.py` — graceful and SIGKILL, per-height
  block-hash comparison.
- **Level E (fork choice):** `fork_choice.py` — deterministic divergent-branch
  convergence without iptables.
- **Level F (platform):** `platform_digest.sh` and the digest matrix in
  `.github/workflows/parity.yml`.
- **Levels H/I:** `security_smoke.py` (RPC auth, oversized body, JSON bomb) and
  `soak.py` (progress, RSS ceiling, restart checkpoint).
- **Level C (fuzz):** four targets (`MiniData`, JSON, `MiniNumber`, `TxPoW`), a
  portable deterministic driver and `tests/fuzz/run_fuzz.sh`, built with
  ASan+UBSan.
- **Property tests:** `test_tree_properties.cpp` (weight conservation, heaviest
  child, idempotence, ancestry/pruning) and `test_txpowtree_weight.cpp`.
- `tests/parity/README.md` documents every level, its parameters and its limits.

### Added — release gate

- `minima-tests` builds every `test_*` target and `ctest` runs all of them with
  no path filter; `tests/check_ctest_registration.py` fails the build when a
  test executable has no `add_test`.
- 11 test executables that existed but were never registered are now registered
  (`winternitz`, `serialization`, `serialization_standalone`, `mmr`,
  `consensus_vectors`, `mempool_index`, `sql_batch`, `script_conformance`,
  `archive_roundtrip`, `archive_records`, `archive_tail`) and the missing
  `test_serialization_standalone` target was added.
- `test_archive_roundtrip` covers the archive writer/reader (schema, chunk
  flush, reopen, duplicate idempotence, batch commit, gap detection) without a
  fixture; the two real-archive validators report `SKIP` (77) when run without
  one.
- Parity runs on hosted runners and no longer fails on the unavailable
  `macos-14` runner; the digest comparison requires at least two platforms and
  asserts they agree.

### Fixed

- `COMMAND_REFERENCE.md` was stale: the `state` command and four `archive`
  parameters were missing, and `balance`/`coins` had the wrong access level.
- v0.5.13 artifacts announced `0.5.12`: the product version was hardcoded in
  `GlobalParams::MINIMA_MORE_VERSION` while `status` used the CMake define. It is
  single-sourced from `project()` (with a literal fallback for standalone test
  builds), so an artifact cannot drift from its tag.

### Known gaps (documented, not fixed)

- IBD carries no transaction bodies, so witness/ownership signatures cannot be
  verified at insertion (coin proofs and the committed MMR are).
- Unsolicited IBDs are accepted without an outstanding sync request.
- The empty-tree root is PoW-gated but there is no trusted genesis/checkpoint.

### Compatibility

- Behaviour changes only for invalid blocks. Valid blocks, wire format, storage
  and archive files are unchanged.

## v0.5.13 — Operations rollback tooling and persistence fixes

### Added

- restorecascade action to adopt the cascade stored in the archive DB
- restoretree action to rebuild the live tree from the archive

### Fixed

- rethrow corrupt database load errors
- gap-aware peer backfill; preserve MMR; guard null MMR root
- catch up the MegaMMR from archive blocks in replay mmr:true
- cover the live tree tail in archive-block MMR catch-up
- reconcile cascade to the tree root from the archive after recalc
- persist the MegaMMR immediately after replay catch-up
- reset the tree root before rebuilding from the archive
- atomic chaintree save, refresh node count, recover corrupt tree on load
- serialize archive IBD vs save, persist reachable count, safe restoretree
- skip block generation when the tip has no MMR root
- serialize live peer IBD against periodic save
- lock all live tree mutation and replay against state save
- make the txpow SQL lock recursive to avoid re-open self-deadlock
- do not blacklist a peer when our tip is outside its 512-block pulse window
- request sync-IBD when a peer is ahead so catch-up actually starts
- make state save exclusive and serialize miner/pulse tree reads
- do not auto-mine on archive nodes
- own a deep copy of witness coins instead of a non-owning shared_ptr
- correct /v1/scripts/{hash} offset

## v0.5.12 — P2P peer DB load fix

### Fixed

- **P2P failed to load its peer list (`bad any_cast`).** The P2P DB stores peers
  as `{"host","port"}` objects; the JSON parser returns
  `std::shared_ptr<JSONObject>` for objects, but `addressesJSONToList` only
  accepted `JSONObject`. As a result P2P_INIT/P2P_LOOP and the `peers` command
  threw `bad any_cast` and the node had no peers. It now accepts both, plus
  legacy `host:port` string entries.
## v0.5.11 — Fail-closed CoinProof deserialization

- `CoinProof::ReadFromStream` now throws on truncated/corrupt proof data instead
  of returning `nullptr`.
- `TxBlock` and `Witness` reject a null CoinProof with a typed serialization
  error.
- `TxPowTree::readDataStream` aborts the load on a node read failure rather than
  running with a partial/corrupt tree.
- A corrupt tree now reaches a clean CoinProof read error instead of dereferencing
  null and raising `SIGSEGV`.
## v0.5.10 — MMR catch-up in replay

### Added

- `archive action:replay mmr:true [mmrfrom:<h>]` calls `MegaMMR::addBlock`
  in chain order from the MMR block time (default) or the supplied height to
  the chain tip. Use it when an archive/tree restore left the MegaMMR behind;
  without this the node rejects new blocks because the MMR is missing entries.
## v0.5.9 — Ops rollback tooling and consistency checks

### Added

- `state action:verify` — one read-only report across archive blocks, cascade,
  TxPoWTree and MegaMMR: `ok`, `issues`, contiguous range, `cascade_tip`,
  `tree_root`/`tree_tip`, `gap_blocks`, `mmr_coins`, `version`.
- `archive action:verify` — the same report through the archive command.
- `archive action:replay` — re-append missing archive blocks from the local
  cascade **and then the tree window** (`getHeaviestBranch()`), so the archive
  reaches the chain tip. Clamps to the archive tip and returns a no-op when
  already current.
- `scripts/snapshot.sh` — quiesce, `sqlite3 .backup`, coupled-store copy,
  `SHA256SUMS` + `manifest.json`, optional `-nop2p` validation via
  `state verify` on ports 19001/19005.
- `scripts/state-watch.sh` — one-line consistency probe with exit status.

### Fixed

- `ArchiveManager::checkCascadeRequired` compared the cascade tip against an
  inverted archive range (`archstart` was the last block), so it always
  re-saved the cascade and never surfaced a block gap. It now checks the real
  range and logs `ARCHIVE_GAP` with the replay hint.
- Startup logs `STATE_INVALID: <issues>` when archive/cascade/tree/MMR diverge
  (never blocks startup).
- `TOTEM_NODE_VERSION` is defined on `minima_core`, so library code can report
  the product version.
## v0.5.8 — MMR null-entry hardening

### Fixed

- **SIGSEGV when an MMR entry is missing.** `MMREntry::getMMRData()` returns
  `nullptr` for empty entries, and `MMR::addEntry`/`updateEntry` dereferenced it
  unconditionally. The Java node throws `NullPointerException` at the same point
  and the block is rejected; the C++ port dereferenced null and crashed the
  process (observed in `MiniNumber::align_scales` when a restored/incomplete
  MegaMMR produced an empty sibling). The MMR now throws a catchable
  `std::runtime_error` (`MMR: missing MMRData ...`) so the TxPoW processor
  rejects the block and the node keeps running. The MMR sources themselves are
  unchanged since v0.5.0 — this is a latent porting bug exposed by inconsistent
  MMR data.
## v0.5.7 — RPC error reporting

### Fixed

- **Command errors were reported as empty strings.** `CommandRunner` stored
  `CommandException::what()` (a `const char*`) in the JSON reply, but the JSON
  writer only serialises `std::string` from `std::any`. Every command that
  failed with a `CommandException` returned `"error": ""`, while
  `std::exception` failures (already wrapped in `std::string`) reported
  correctly. This affected all command-level validation errors, e.g.
  `archive action:importraw` on a missing file and `archive action:export` with
  a bad `index:` directory.

### Changed

- `analytics/SPEC.md` no longer advertises `GET /v1/export`; it was never
  implemented and is not part of this node's public API.

### Notes

- No consensus, wire-format, storage or API-shape changes. Error strings are the
  only behavioural difference.

## v0.5.6 — IBD, archive and analytics fixes

### Fixed

- **IBD chain contest never ran.** The v0.5.4 fresh-node sync fix set
  `Main::setSyncIBD(true)` in the NIO `MSG_IBD` handler, but `isSyncIBD()` was
  also the guard around `IBD::checkOurChainHeavier()`, so the guard was always
  true and heavier-chain rejection was dead. Chain acceptance is now decided
  from the local chain (`tree root == nullptr || cascade length == 0`), which
  restores the Java behaviour, and the flag is `std::atomic<bool>` because it is
  written by both the NIO and processor threads.
- **Stuck sync mode.** The flag is cleared on every early exit from the
  `MSG_IBD` handler (invalid IBD, heavier-chain disconnect) and in the
  processor's outer `catch`, so a rejected or throwing IBD no longer leaves the
  node permanently in sync mode.
- **Legacy archives were unreadable.** The analytics reader always selected
  `chunkid, offset, len`, so any pre-v0.5 or Java-written archive failed with
  `no such column: chunkid`. Both layouts (chunked and per-row `syncdata`) are
  detected and read; completeness reporting handles either.
- **Truncated raw archives looked like success.** A short or corrupt gzip
  stream returned end-of-file, so `archive action:import` reported
  "Archive sync completed" after importing a prefix. Corruption, a missing
  end-of-stream marker and short reads now fail explicitly, including in the
  `mysql` import and the analytics raw path.
- **A fresh analytics index always aborted.** `buildIndex` ran the magic
  backfill and then appended the same heights from the block pass, and
  `magic_snapshots.height` is a primary key. The main pass now owns magic on a
  fresh index.
- **Silent archive scan aborts.** `forEachArchiveBlock` discarded the final
  `sqlite3_step` result, so `SQLITE_BUSY` (node writing a chunk, checkpoint,
  vacuum), `SQLITE_IOERR` or corruption ended the scan exactly like a clean
  end-of-data and truncated the index at an arbitrary height. Failures now
  report the height, return code and message.

### Added

- Every analytics index pass prints the archive range, storage layout and
  completeness, then the indexed height and lag, so an apparent "stops at
  block N" can be attributed to a live tip, a pruned archive, or a real stop.
- `AGENTS.md`, and an import-order callout plus bootstrap URLs in `README.md`.

### Notes

- `MAX_KEEP_BLOCKS` (`2000 × NUMBER_DAYS_ARCHIVE`, default 100,000) prunes the
  archive to a sliding window unless `-archive` is set; an index that appears to
  stop often has `MIN(block) = tip - 100,000`.

## v0.5.5 — conformance & security hardening

### Conformance & security (external review follow-up)

- `MiniNumber` preserves the Java scale for values truncated past 44 decimals
  and for written zeros (`0.0`, `0E+5`), so serialized bytes match the Java
  node (`1E-45` serializes as `2C0100`).
- Hostile exponents no longer trigger unbounded `pow10` or string expansion;
  `1E+2147483647` is rejected and `1E-999999999` resolves to `0E-44`
  immediately.
- `LEN`/`SUBSTR` operate on UTF-16 code units like Java (`LEN([é]) == 1`;
  surrogate pairs count as two units).
- Expressions start at stack depth 0, so 64 nested brackets are accepted and
  65 rejected, matching Java.
- `keys action:list|new` no longer returns `privatekey` (Java `KeyRow.toJSON`
  omits it; only `genkey` returns key material).
- `txncheck` and `TxPoWGenerator` reject a missing chain tip instead of
  dereferencing null.
- Peer-check workers are reaped during runtime and worker exceptions are
  contained; RPC request-line size is capped (POST bodies were already capped).
- Regression coverage added to `test_script_conformance`; MiniNumber
  parse/format/wire and ordering differentials match `minima.jar` 1.0.45.15.

## v0.5.1 — testnet block production + hardening

### Fixed

- Test/solo mode now reschedules `TXPOWMINER_MINEPULSE` after each mined block,
  so fresh genesis/funding coins confirm continuously without `automine` or a
  node restart. Mainnet/consensus behavior is unchanged.
- Java-compatible `MiniNumber::getAsInt()` low-32-bit truncation.
- Normal `MiniNumber` values enforce the Java 2^64−1 bound; chain-weight totals
  retain a separate wide path.
- Uppercase `0X` is no longer accepted as a hex literal.
- NIO frames no longer reserve the full declared wire size; per-type frame
  ceilings, per-peer backpressure and an 8-connection per-IP inbound cap are
  active. Global 256 MB NIO / 512 MB MiniData ceilings remain.

### Added

- `minima-analytics -megammr` MegaMMR-only mode: indexes the unpruned recent
  coin set and MMR root without an archive, and reports `index_state=ready`
  instead of failing on empty archive-backed metric tables.
- `analytics/tests/mmr_only_smoke.sh` live smoke test.

### Fixed

- MegaMMR-only indexing no longer inserts NULL `metric_series.bucket_start_ms`
  rows; archive-backed metrics are skipped when no archive source exists.
- Document the DuckDB catalog/schema collision when the index file is named
  `analytics.duckdb`; use `minima.duckdb` (or any non-`analytics` name).

## v0.5.0 — conformance + analytics

- KISSVM parity fixes: `HEX()` sign-byte removal, Java-identical
  `cleanScript()` spacing, non-recursive token classifiers (long-hex crash).
  18/18 deployed Stables scripts match Java addresses; consensus vectors 12/12.
- Apache-2.0 licensing (`LICENSE`, `NOTICE`, `SECURITY.md`).
- Chunked archive storage, complete import tail, `-ibdlimit` cooldown.
- Optional `minima-analytics`: DuckDB indexer/API, tokens/NFTs/media, scripts,
  magic numbers, HODL waves, burn, backup/restore.
- BI templates: **Evidence** (history; MIT) and **Rill** (live/ops;
  Apache-2.0), sharing `analytics/integrations/common/metrics.yaml`.

This report summarises the substantive changes made during the recent development push on the PureMinima C++ node. The goal was to fix correctness/robustness issues, safely improve performance, and prove the code builds cleanly across macOS (Clang) and Linux (GCC).

---

## 1. Consensus-Critical Fixes

These changes preserve the existing consensus rules (SHA3-256, MiniNumber arithmetic, block validation). They only fix cases where a fresh or syncing node incorrectly rejected valid chain data.

### 1.1 Fresh-node IBD rejection

**Problem:** A brand-new node with zero blocks received an Initial Block Download (IBD) from a peer and ran `IBD::checkOurChainHeavier()`. With `myBlocks=0`, `theirBlocks≈2070`, `myCascLen=1`, `theirCascLen=27`, the local weight was roughly 42 million while the peer chain weight was roughly 82 billion. The node therefore rejected the IBD and could never sync.

**Fix:** In `org/minima/system/network/minima/n_i_o_message.cpp`, the weight contest is now skipped while `Main::isSyncIBD()` is true. The node accepts the incoming chain during sync and only contests weights once it is caught up.

**Files:**
- `org/minima/system/network/minima/n_i_o_message.cpp`
- `org/minima/system/network/minima/n_i_o_message.hpp` (added `#include <vector>` for GCC portability)

---

## 2. P2P Robustness

### 2.1 Peer persistence

**Problem:** `P2PParams::DEFAULT_NODE_LIST` is empty. If the bootstrap peer disconnected, a restarted node had no remembered peers to reconnect to.

**Fix:** `P2PManager` now saves every known peer to `p2p.db` on shutdown / peer-list changes and seeds the in-memory `knownPeers` set from that database on startup.

**Files:**
- `org/minima/system/network/p2p/p2_p_manager.cpp`
- `org/minima/system/network/p2p/p2_p_manager.hpp`

### 2.2 Use-after-free in peer lookup

**Problem:** `getClientFromInetAddress()` returned a raw pointer to a `NIOClientInfo` owned by a vector. Callers in the swap-links code could use the pointer after the vector reallocated.

**Fix:** Changed `getClientFromInetAddress()` to return `std::unique_ptr<NIOClientInfo>`, transferring ownership to the caller. Updated all callers in the swap-links path to handle the owned return value.

**Files:**
- `org/minima/system/network/p2p/util_funcs.hpp`
- `org/minima/system/network/p2p/swap_links_functions.cpp`
- `org/minima/system/network/p2p/swap_links_functions.hpp`

---

## 3. Crypto / SSL Modernization

### 3.1 OpenSSL EVP migration

**Problem:** The code used deprecated low-level `RSA_*` APIs. These are removed in newer OpenSSL (3.x) and generate deprecation warnings/errors on modern toolchains.

**Fix:** Migrated RSA key generation and SSL context setup to the OpenSSL EVP API.

**Files:**
- `org/minima/utils/encrypt/generate_key.cpp`
- `org/minima/utils/ssl/s_s_l_manager.cpp`

---

## 4. Database Performance

### 4.1 Centralised SQLite tuning

**Problem:** Each SQLite connection used default pragmas, giving poor I/O throughput (especially under heavy sync).

**Fix:** Added `SqlDB::applySQLiteTuning()` which enables:
- WAL journal mode
- `synchronous=NORMAL`
- Larger cache size
- Busy timeout
- `mmap_size` where supported

The helper is invoked from every database module that opens a SQLite connection.

**Files:**
- `org/minima/utils/sql_d_b.cpp`
- `org/minima/utils/sql_d_b.hpp`
- `org/minima/database/txpowdb/sql/tx_po_w_sql_d_b.cpp`
- `org/minima/database/archive/archive_manager.cpp`
- `org/minima/database/wallet/wallet.cpp`
- `org/minima/database/txpowdb/onchain/tx_po_w_on_chain_d_b.cpp`

---

## 5. MMR (Merkle Mountain Range) Optimisation

### 5.1 Peak/root caching

**Problem:** `getPeaks()` and `getRoot()` rebuilt the peak list and recomputed the root on every call, leading to O(MAXROWS) scans and temporary-MMR rebuilds in hot paths.

**Fix:** Added mutable caches for the peak list and root inside `MMR`. Caches are lazily computed and invalidated by every mutating method (`addEntry`, `finalizeSet`, `updateEntry`, `set/setSet`, etc.). `getPeaks()` and `getRoot()` return identical values to the uncached implementation.

**Files:**
- `org/minima/objects/mmr/m_m_r.hpp`
- `org/minima/objects/mmr/m_m_r.cpp`
- `org/minima/objects/mmr/m_m_r_proof.hpp`
- `org/minima/objects/mmr/m_m_r_proof.cpp` (added copy-assignment)

### 5.2 MMR correctness test

Added a standalone test that exercises the cached paths:
- `addEntry` / `getRoot`
- `getProof` / `checkProof`
- `finalizeSet` / unfinalize
- `updateEntry`
- serialization round-trip

**Files:**
- `tests/test_mmr.cpp`
- `tests/stubs.cpp` (added `MinimaDB` / `MegaMMR` stubs for lightweight linkage)
- `CMakeLists.txt` (`test_mmr` target)

---

## 6. Command System

### 6.1 `help` command

**Problem:** `help` with no arguments did not list the available commands.

**Fix:** Implemented `CommandRunner::getCommandPrototypes()` and updated `help.cpp` to enumerate every registered command, its parameters, and description.

**Files:**
- `org/minima/system/commands/help.cpp`
- `org/minima/system/commands/command_runner.hpp`

---

## 7. Build / Compiler Hygiene

A large pass was made to make the project compile cleanly on macOS Clang with `-Wall -Wextra -Wpedantic` and on Linux GCC 13.

### 7.1 Virtual/override cleanup
- Marked overriding methods with `override` across command classes and KISSVM value classes.
- Removed redundant or conflicting `virtual` specifiers.

**Files (representative):**
- `org/minima/system/commands/*/*.hpp`
- `org/minima/kissvm/values/*.hpp`

### 7.2 Move semantics / implicitly-deleted members
- Replaced dangerous `= default` move constructors/operators with explicit `= delete` where the class contains members that make moving unsafe or unsupported.

### 7.3 Initialiser-order warnings
- Reordered member initialisers to match declaration order in:
  - `org/minima/objects/tx_po_w.cpp`
  - `org/minima/objects/mmr/m_m_r_data.cpp`
  - `org/minima/system/network/minima/n_i_o_client.cpp`

### 7.4 Missing include
- Added `#include <vector>` to `org/minima/system/network/minima/n_i_o_message.hpp` so GCC no longer fails to see `std::vector` in the class declaration. This is a canonical example of code that was never strictly correct but happened to compile because AppleClang pulled in the header transitively.

### 7.5 CMake / build environment
- Added standalone test targets: `test_winternitz`, `test_serialization`, `test_mmr`.
- Added `Dockerfile.linux-build` for a reproducible Ubuntu 24.04 / GCC 13 build environment.
- Added `build-linux-x86_64-toolchain.cmake` (experimental Zig cross-compilation toolchain file).

### 7.6 Why this matters for machine-code implementations

A blockchain node is delivered as native machine code, not as C++ source. Different compilers and standard libraries produce different binaries from the same source. If the node only builds on macOS/Clang, there is only one proven machine-code path, and any hidden source-level bug becomes part of that binary.

Porting to Linux/GCC:
- Surfaces toolchain-specific assumptions (missing includes, different header graphs, different warnings).
- Provides the platform the node will actually run on in production.
- Removes toolchain monoculture risk: if a future Clang or macOS change breaks the build, the GCC/Linux path remains viable.

For consensus, this is important because language-level bugs (undefined behaviour, incorrect overrides, incomplete types) can compile into different runtime behaviour on different toolchains. Catching them on both Clang and GCC reduces the chance that the Linux binary and the macOS binary diverge on block validation.

See `PORTABILITY.md` for a deeper discussion.

---

## 8. Validation

### 8.1 macOS (Apple Silicon, Clang)
- `make minima` builds with **zero warnings**.
- Tests pass:
  - `test_winternitz` (5/5 cases)
  - `test_serialization`
  - `test_mmr`

### 8.2 Linux (Docker, Ubuntu 24.04, GCC 13, aarch64)
- Full `minima` executable builds and links.
- libzip and MySQL features are disabled in this container because their packages are broken/missing; core node functionality is unaffected.
- Same three test executables build and pass inside the container.

---

## 9. Files Changed Summary

The key source files touched are:

| Area | Files |
|------|-------|
| IBD / networking | `n_i_o_message.cpp/.hpp`, `main.cpp` |
| P2P | `p2_p_manager.cpp/.hpp`, `util_funcs.hpp`, `swap_links_functions.cpp/.hpp` |
| Commands | `help.cpp`, `command_runner.hpp` |
| Crypto/SSL | `generate_key.cpp`, `s_s_l_manager.cpp` |
| SQLite tuning | `sql_d_b.cpp/.hpp`, `tx_po_w_sql_d_b.cpp`, `archive_manager.cpp`, `wallet.cpp`, `tx_po_w_on_chain_d_b.cpp` |
| MMR | `m_m_r.cpp/.hpp`, `m_m_r_proof.cpp/.hpp`, `m_m_r_data.cpp/.hpp` |
| Compiler hygiene | many `commands/*/.hpp`, `kissvm/values/*.hpp`, `tx_po_w.cpp`, `n_i_o_client.cpp`, `m_m_r_data.cpp` |
| Tests | `test_mmr.cpp`, `test_serialization.cpp`, `test_winternitz.cpp`, `stubs.cpp` |
| Build | `CMakeLists.txt`, `Dockerfile.linux-build`, `build-linux-x86_64-toolchain.cmake`, `PORTABILITY.md` |

---

## 10. What Was Explicitly *Not* Changed

To protect consensus, the following were left untouched:
- Hash function: still SHA3-256.
- MiniNumber encoding / arithmetic.
- Block, TxPoW, and transaction validation rules.
- MMR hash algorithm or proof format (caching only stores already-computed values).

---

## 11. Next Suggested Steps

1. (Optional) Build an `x86_64` Linux binary via Docker multi-arch or a cross toolchain.
2. Run a longer mainnet/regtest P2P soak test to confirm the peer-persistence and segfault fixes under real load.
3. Re-enable `libzip` in the Linux build once the Ubuntu `libzip-dev` cmake config is fixed.
