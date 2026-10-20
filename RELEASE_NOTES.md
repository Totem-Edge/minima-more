# Minima More v0.2.0

This release hardens the node, fixes legacy data import (the user-reported
blocker), and cuts mainnet memory dramatically. It also adds the capability
manifest and the in-node MCP server.

## Highlights

- **Legacy imports now work**: MegaMMR (`megammr.mmr` and Minima More
  `.megammr`), raw archives (`.raw.dat`), and legacy **H2 `.gzip`** archive dumps.
- **~70–95 MB less RSS** on a synced mainnet node via role-based SQLite caches
  and returning free glibc arenas to the OS.
- **In-node MCP** (stdio + HTTP/SSE) generated from a capability manifest over
  all **111 commands**.
- **SSRF hardening** for outbound fetches and a tightened RPC read-only allowlist.

## Security

- **Bounded fetcher for outbound URLs** (`RPCClient::sendGET`): HTTP/HTTPS only,
  rejects loopback/private/link-local/CGNAT/ULA hosts, max 3 redirects, 15 s
  timeout, 1 MiB response cap.
- **RPC read-only allowlist tightened**:
  - removed `vault` (seed phrase exposure), plus `getaddress`, `keys`,
    `runscript`, `megammr`;
  - removed `peers` (its `url:` action was an SSRF vector).

## Import & data migration

- **MegaMMR import fixed** (root cause): `MegaMMRBackup::readDataStream`
  required pre-set `MegaMMR`/`IBD` objects, and `loadObjectSlow()` swallowed the
  resulting exception — so every `megammr action:import` silently failed.
  - Now allocates its own objects (matching the Java reference).
  - Auto-detects **versioned `.megammr`** vs **legacy raw `megammr.mmr`**; the
    reply reports `legacyraw`.
  - `megammr action:integrity` validates either form and handles a missing IBD.
- **Legacy H2 archive import**: `archive action:import file:x.gzip` now reads the
  gzipped **H2 SQL script** (`SCRIPT … COMPRESSION GZIP`, `SYNCBLOCK` /
  `CASCADEDATA`, `X'…'` blobs), converts it to the raw archive stream, and reuses
  the normal resync flow. This is a focused archive-schema reader, not a general
  H2 engine.
- **Raw archive import** verified for both implementations; `raw:true` forces raw
  parsing for renamed files.

## Memory & performance

Measured on one host: AWS `c6g.xlarge` (4 vCPU / 8 GB / Neoverse-N1).

- **Role-based SQLite cache**: 4 MiB (`p2p`/`userprefs`), 8 MiB (hot state),
  16 MiB (archive) instead of 16 MiB × 6 connections.
  - Mainnet **anonymous RSS 224 MB → 160 MB** at the same block height.
  - Env: `MINIMA_SQLITE_CACHE_KIB`, `MINIMA_SQLITE_MMAP_MB`.
- **`malloc_trim(0)` after key generation + periodic checker/cleanup**:
  - steady-state mainnet RSS **261–282 MB → ~196 MB** automatically;
  - `memreport trim:true` reaches **~169 MB** (freed glibc arenas returned to
    the OS).
- **Bounded startup key precompute**: 16 keys (≤2 GB), 32 (≤8 GB), 64 (larger);
  `MINIMA_INIT_KEYS` overrides.
- **Archive cache** on non-archive nodes: 16 MiB/128 MiB → 4 MiB/32 MiB.
- **Optional jemalloc**: `-DMINIMA_USE_JEMALLOC=ON`; A/B showed **−35 MB**
  steady-state (219 → 180 MB).
- **`memreport`** command + `-memreport` flag: process RSS (total/anonymous/file),
  per-database sizes incl. WAL, byte attribution for tree/cascade/RAM TxPoW/MMR,
  glibc arena/in-use/free, `trim:true`, and `dump:true` (jemalloc profile).

## Command manifest & in-node MCP

- **Capability manifest** generated from the registry: node header (version,
  network, mode, serves) + **111 commands** with category, annotations
  (`readOnly` / `destructive` / `idempotent` / `sensitive`) and typed params.
- **`capabilities`** command, **`-manifest`** / **`-manifest md`**.
- **In-node MCP**: `initialize`, `tools/list`, `tools/call`, `ping`,
  notifications.
  - `-mcp stdio` (logs routed to stderr; stdout carries only MCP frames).
  - `-mcp http`: `POST /mcp` and `GET /mcp` SSE on the RPC port, after the RPC
    authorisation check; read-only credentials are refused.
- **Generated docs**: `COMMAND_REFERENCE.md` (regenerate with
  `gen_command_reference`; `test_docs_manifest` prevents drift).

## Correctness

- **JSON writer**: `put(key, "literal")` no longer emits unquoted values
  (`"type":simple`); `const char*` is now quoted/escaped.
- **Node flags**: `-mcp` (and `-memreport`) are stripped before
  `ParamConfigurer`, fixing the “Unknown argument” startup failure.
- **Benchmark harness**: Java classpath resolution walks up to
  `reference/minima-core` (fixes `NoClassDefFoundError: org/h2/Driver`); Java pid
  resolved after `-daemon` fork; `MODE=full|archive|megammr` support.

## Benchmarks & docs

- One-host results on **AWS `c6g.xlarge` (4 vCPU / 8 GB / Neoverse-N1)**: live
  mainnet, core crypto/MMR, lifecycle, and storage engines.
- README rewritten around those numbers; all SVG charts and the hero infographic
  regenerated.
- Checked-in results: `reference-bench/results/aws8-{mainnet,core,sweep,engines}`.

## Tests

- `test_capabilities` **11/11**
- `test_mcp_server` **23/23**
- `test_docs_manifest` **2/2**
- `test_megammr_import` **6/6**
- `test_h2_import` **5/5**
- Existing gates still pass: `test_consensus_vectors`, `test_mmr`,
  `test_winternitz`, `test_mempool_index`, `test_sql_batch`.

## Upgrade notes

- No on-disk format change for wallet/chain; existing data directories carry
  over.
- New flags/commands: `-manifest`, `-mcp stdio|http`, `memreport`, `capabilities`.
- To import legacy data: `megammr action:import file:…` and
  `archive action:import file:….gzip` (or `.raw.dat`).
- No config migration required; new environment knobs are optional.

## Known issues / in progress

- **Fully static release variant** is in progress (a minimal static libcurl is
  being used to avoid the distro’s GSSAPI dependency). Standard tarballs are
  unaffected.
- The H2 importer covers the **SQL script** export
  (`SCRIPT … COMPRESSION GZIP`); a raw H2 database file (`.mv.db`) requires
  running `SCRIPT TO` on the legacy node first.
- Archive `exportraw` on a node whose archive has no `syncblock` rows beyond the
  cascade can produce a cascade-only file (data-dependent; logic matches the
  Java reference).

## Install

Download the tarball for your architecture and verify the checksum:

```sh
tar -xzf minima-v0.2.0-linux-<arch>.tar.gz
sha256sum -c minima-v0.2.0-linux-<arch>.tar.gz.sha256
./minima-v0.2.0-linux-<arch> -manifest
```

## Full changelog (since v0.1.0)

```
57048ad feat(archive): import legacy H2 .gzip archive dumps
6ec50bd fix(import): legacy + Minima More MegaMMR/archive import
dc4f324 perf(memory): return freed glibc arenas to the OS (malloc_trim)
8ba97ef bench(harness): add MODE=full|archive|megammr support
21ac279 perf(sqlite): shrink the archive cache on non-archive nodes
0040d0b feat(memory): add live memreport command + -memreport flag
3037439 perf: role-based SQLite cache budget, bounded key precompute, optional jemalloc
013fb02 feat(rfc-0011): generated capability manifest + in-node MCP server
7747340 docs(readme): rewrite around the one-host 8 GB Graviton results; regenerate all figures
7e4acc6 bench(results): core crypto/MMR C++ vs Java on AWS c6g.xlarge (aarch64)
d58de20 bench(results): live-mainnet C++ vs Java on AWS c6g.xlarge (4 vCPU/8GB, Neoverse-N1)
a30f64b bench(env): locate reference/minima-core by walking up, not fixed ../..
c1bc6c9 fix(json): quote/escape const char* values in JSON writer
8120f1a bench(mainnet): resolve real node pid after -daemon fork so java metrics populate
42043bd security(ssrf): bounded fetcher in RPCClient::sendGET
3030072 security(ssrf): drop 'peers' from RPC read-only allowlist
ccbc1b6 security: remove vault (and getaddress/keys/runscript/megammr) from RPC read-only allowlist
```
