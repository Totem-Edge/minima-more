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
