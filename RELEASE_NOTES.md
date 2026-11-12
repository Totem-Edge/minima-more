# Minima More v0.6.0 — IBD validation fix and graceful termination

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

# Minima More v0.5.14 — IBD consensus hardening, parity harness and a real test gate

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

Full history, including the previously undocumented v0.5.13 release, is in
[`CHANGELOG.md`](https://github.com/Totem-Edge/minima-more/blob/main/CHANGELOG.md).

# Minima More v0.5.13 — Operations rollback tooling and persistence fixes

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

# Minima More v0.5.12

P2P peer-list load fix.

## Fixed

- The P2P DB reader cast peer entries to `JSONObject`, but the JSON parser
  returns `std::shared_ptr<JSONObject>` for objects, so P2P init/loop and
  `peers` failed with `bad any_cast` and the node ran with no peers. Both
  representations (and legacy `host:port` strings) are now accepted.

## Compatibility

- No consensus, wire-format or storage changes.
# Minima More v0.5.11

Fail-closed deserialization for corrupt proofs and trees.

## Fixed

- `CoinProof::ReadFromStream` throws on truncated/corrupt proof data instead of
  returning `nullptr`. `TxBlock` and `Witness` previously dereferenced that
  null, which is the SIGSEGV seen when a partially-written tree was loaded.
- `TxBlock` and `Witness` reject a null CoinProof with a typed serialization
  error.
- `TxPowTree::readDataStream` aborts the load on a node read failure instead of
  continuing with a partial/corrupt tree.

## Compatibility

- No consensus, wire-format or storage changes for valid data. Valid blocks and
  proofs serialize identically. A corrupt input now fails cleanly instead of
  crashing.
# Minima More v0.5.10

MMR catch-up in archive replay.

## Added

- `archive action:replay mmr:true [mmrfrom:<h>]`: replay the local cascade +
  tree window into the MegaMMR, starting at its own block time (or a supplied
  height). Fixes the case where the archive/tree were restored but the MMR was
  left behind, which stalls new block processing.

## Compatibility

- No consensus, wire-format, storage or API-shape changes.
# Minima More v0.5.9

Ops rollback tooling and consistency checks.

## Added

- `state action:verify` and `archive action:verify`: a single report covering
  archive contiguity/range, cascade validity, tree alignment and the archive
  gap, with `ok:true/false` and an `issues` string.
- `archive action:replay`: fill a missing archive range from the local cascade
  and tree window, up to the chain tip, without touching the MMR.
- `scripts/snapshot.sh` (quiesce + SQLite backup + manifest + optional
  `-nop2p` validation) and `scripts/state-watch.sh` (probe for timers).

## Fixed

- `checkCascadeRequired` inverted range check; it now reports `ARCHIVE_GAP`
  instead of silently re-saving a cascade ahead of the archive.
- Startup `STATE_INVALID` log when coupled stores diverge.

## Compatibility

- No consensus, wire-format, storage or API-shape changes. Safe drop-in for
  v0.5.8. New commands only.
# Minima More v0.5.8

MMR null-entry hardening.

## Fixed

- A node with an incomplete/corrupt MMR entry crashed with SIGSEGV when the
  MMR tried to build a parent node from an empty sibling. The C++ port returned
  a null `MMRData*` and dereferenced it; Java throws a caught
  `NullPointerException` instead. The MMR now throws a catchable
  `std::runtime_error`, so the block is rejected and the node continues.

## Compatibility

- No consensus, wire-format, storage or API-shape changes. Safe drop-in for
  v0.5.7. The MMR code is unchanged since v0.5.0; this only changes the
  failure mode on inconsistent MMR data from a crash to a rejected block.
# Minima More v0.5.7

Error-reporting patch.

## Fixed

- RPC command failures returned an empty `error` string when the command threw
  a `CommandException`. The message is now included — e.g.
  `archive action:importraw file:<missing>` reports
  "Restore file doesn't exist : ..." instead of `""`. This affects every
  command-level validation error, not just archive.

## Changed

- Removed the unimplemented `GET /v1/export` row from `analytics/SPEC.md`.

## Compatibility

- No consensus, wire-format, storage or API-shape changes. No new endpoints,
  commands or flags. Safe drop-in for v0.5.6.

# Minima More v0.5.6

Archive, IBD and analytics fixes.

## IBD / networking

- **Heavier-chain rejection is active again.** v0.5.4 disabled it accidentally
  by setting the sync flag inside the IBD message handler, which also guarded
  the chain-weight contest. Acceptance is now decided from the local chain,
  matching the Java node.
- `isSyncIBD()` is atomic and is cleared on rejected and throwing IBDs, so a
  node can no longer get stuck in sync mode after a bad peer.

## Archive

- **Legacy archives read correctly.** Pre-v0.5 and Java-written archives store
  blocks per row; the analytics reader assumed chunked storage and failed with
  `no such column: chunkid`. Both layouts are now supported.
- **Truncated `.raw.dat` imports fail loudly.** A cut-off download used to
  import its prefix and report success; corrupt gzip, a missing end-of-stream
  marker and short reads are now errors.
- **A fresh analytics index no longer aborts** on a duplicate
  `magic_snapshots` key.

## Analytics

- Archive scan failures (`SQLITE_BUSY`/IO) report the block, code and message
  instead of silently ending the pass.
- Every pass prints the archive range, storage layout and completeness, plus the
  indexed height and lag.

## Operator notes

- Import **MegaMMR first, archive second** — never the reverse, never
  concurrently.
- `-archive` is required for an unpruned archive; otherwise only the last
  `2000 × NUMBER_DAYS_ARCHIVE` blocks (default 100,000) are retained.

Verified: 12/12 consensus vectors, script conformance, ctest 5/5, and an
85,211-block legacy archive indexes end to end (full pass plus a 0-block rerun).

# Minima More v0.5.5

Conformance and security hardening release, following an external review of
v0.5.1. Every behavioural change below makes the node agree with the Java
reference implementation or closes an availability/key-exposure issue.

## Consensus / wire-format parity

- **MiniNumber scale:** values truncated past 44 decimals and written zeros keep
  the Java scale, so serialized bytes match the Java node
  (`1E-45` → `2C0100`, `0.0` → `010100`, `0E+5` → `FB0100`).
- **Nested expressions:** expressions start at stack depth 0, so 64 nested
  brackets are accepted and 65 rejected, exactly as in Java.
- **Strings:** `LEN` and `SUBSTR` operate on UTF-16 code units
  (`LEN([é]) == 1`; surrogate pairs count as two units), matching Java
  `String.length()`/`substring()`.

Validated against `minima.jar` 1.0.45.15: 191-value parse/format/wire
differential and pairwise ordering matrix match exactly; 12/12 consensus
vectors and `test_script_conformance` pass.

## Security / availability

- Hostile exponents no longer trigger unbounded `pow10` or string expansion
  (`1E+2147483647` is rejected quickly; `1E-999999999` resolves to `0E-44`).
- `keys action:list|new` no longer returns `privatekey`; only `genkey` returns
  key material.
- `txncheck` and `TxPoWGenerator` reject a missing chain tip instead of
  dereferencing null.
- Peer-check workers are reaped during runtime and worker exceptions are
  contained, preventing unbounded thread accumulation.
- RPC request-line size is capped (POST bodies remain capped).

# Minima More v0.5.1

Testnet block-production and hardening release.

## Fixed

- Test/solo mining now continues after each block; funding coins confirm without
  `automine` or restarts.
- Java-compatible `MiniNumber::getAsInt()` low-32-bit truncation.
- Normal `MiniNumber` values now enforce the Java 2^64−1 bound; wide chain-weight
  totals retain their separate internal path.
- Uppercase `0X` is no longer accepted as a hex literal.

## Security / availability

- NIO frames no longer allocate the full declared size up front.
- Per-message-type frame ceilings are enforced.
- Per-peer backpressure and an 8-connection per-IP inbound cap are active.
- The 256 MB NIO and 512 MB MiniData compatibility ceilings are retained.

No consensus or wire-format change.

# Minima More v0.5.0

The conformance + analytics release. Apache-2.0 licensed.

## Highlights

### Script conformance fixes (consensus-critical)

- **`HEX()` now matches Java**: `HEX(255)=0xFF`, `HEX(128)=0x80`,
  `HEX(0)=0x00`, `HEX(256)=0x0100` (the leading `0x00` sign byte is removed).
- **`cleanScript()` now matches Java exactly**, including operator spacing.
  All **18 deployed Stables vault/contract scripts** derive the exact Java
  addresses, including all 8 vault contracts.
- **Long-hex crash fixed**: token classifiers no longer use recursive
  `std::regex`; a 12,000-byte hex script parses, cleans and executes.
- Regression tests: `test_script_conformance`, `test_consensus_vectors` 12/12.
- **Apache-2.0**: `LICENSE`, `NOTICE`, `SECURITY.md`, and provenance in README.

### Archive

- **Chunked cross-block compression**: 256 blocks/chunk (`ZCHN`), 3.83x on the
  full archive; legacy per-row `ZBLK` rows still read.
- **Complete imports**: the final cascade-depth tail is saved, giving
  **2,376,119 blocks / 0 gaps / 0 missing chunks** on the full archive.
- `archive action:integrity`: **2,376,119 blocks, 0 errors**.
- Record battery: 260 heights, parent linkage, ID lookup, round-trip and
  chunk-boundary ranges pass (~2 ms average).
- **IBD cooldown fixed**: `-ibdlimit N` (default 30, `0` = unlimited) now
  actually expires; the old set never did.

### `minima-analytics` sidecar (optional)

- Archive → DuckDB indexer; portable `-backup` / `-restore`.
- Read-only API: capabilities/catalog, blocks, txs, addresses, tokens, scripts,
  magic numbers, consistency, UTXO distribution, HODL waves, cohorts, metrics.
- Full index measured: **2,376,119 blocks**, 12.5 min, 2.7 GB DuckDB,
  2.86 GB peak RSS.
- **8,858 tokens / 7,851 NFTs** (decimals 0); **1,167 embedded NFT images**
  decoded and served (`/media.bin`), plus URL media metadata.
- **320,015 script hashes**; known-script registry seeded from
  `minima-global/MiniDAPP`.
- **Magic numbers** indexed per block (`max_txpow_size`, `max_kissvm_ops`,
  `max_txns`, `min_work`) with `/v1/magic` and change history.
- Burn, spent-age/ASOL, CDD/CDD-90/CYD, dormancy, address activity and UTXO
  percentiles. Minima has no fee market, so fee metrics are not included.
- **Backup**: full 4.4 GB DuckDB → **1.2 GB Parquet in 4.7 s**, with
  `analytics_manifest.json`; restore verified at 2,376,119 blocks.

### BI templates (optional)

- **Evidence** (`analytics/integrations/evidence/`, **MIT**) — historical
  reports: blocks/txs/burn, HODL waves, CDD, UTXO/cohorts, tokens/NFTs, scripts.
- **Rill** (`analytics/integrations/rill/`, **Apache-2.0**) — live/ops
  dashboards: tip/lag, recent blocks, burn, magic changes, script activity,
  alerts; attaches the DuckDB index.
- Shared metric ownership/refresh contract:
  `analytics/integrations/common/metrics.yaml`.

## Artifacts

- `minima-more-v0.5.0-linux-{x86_64,aarch64}` — standard dynamic node
- `minima-more-v0.5.0-linux-{x86_64,aarch64}-static` — fully static node
- `minima-more-analytics-v0.5.0-linux-{x86_64,aarch64}` — sidecar + bundled
  `libduckdb.so`
- `.sha256` for every tarball

## Upgrade notes

- Archive DBs remain compatible; old per-row blobs are read transparently.
- `-ibdlimit 0` is intended for trusted peers/benchmarks; default remains 30 min.
- Analytics requires DuckDB; the release tarball bundles `libduckdb.so` with an
  `$ORIGIN` rpath. Node builds are unchanged (`MINIMA_BUILD_ANALYTICS=OFF`).
- MMR enrichment reports `comparable=false` when the MMR snapshot is older than
  the archive tip; it is not a corruption signal.

## Verify

```bash
tar -xzf minima-more-v0.5.0-linux-<arch>[-static].tar.gz
sha256sum -c minima-more-v0.5.0-linux-<arch>[-static].tar.gz.sha256
./minima-more-v0.5.0-linux-<arch> -manifest

tar -xzf minima-more-analytics-v0.5.0-linux-<arch>.tar.gz
./minima-more-analytics-v0.5.0-linux-<arch> -help
```

License: Apache-2.0. See `LICENSE`, `NOTICE`, and `SECURITY.md`.
