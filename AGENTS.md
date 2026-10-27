# AGENTS.md

Operational rules for working in this repository (Minima More node, v0.5.6+).

## Archive and MMR imports — order is mandatory

Always import **MegaMMR first, archive second**. Never reverse the order and
never run both imports concurrently.

```sh
megammr action:import file:<path>      # 1. wait for completion
archive action:import file:<path>      # 2. only after (1) finishes
```

Any other order is unsupported and may leave the node needing a full
re-derivation. The same note is in `README.md` under "Importing legacy node
data".

## Canonical bootstrap records

Defaults live in `org/minima/system/params/general_params.cpp` and are used when
an import runs without an explicit `file:`.

| Record | URL | Observed 2026-10-27 |
|---|---|---|
| MegaMMR | `https://eurobuddha.com/mega.mmr` | ~492 MiB |
| Archive (raw) | `https://eurobuddha.com/archive.raw.dat` | ~1.42 GiB, **2,386,519 blocks** declared in the header |
| Peer list | `https://eurobuddha.com/minimapeers.txt` | text list |

- `megammr action:import` with no `file:` downloads `MEGAMMR_URL` (2 GiB cap)
  and requires `-megammr` to be enabled.
- `archive action:import` with no `file:` downloads `ARCHIVE_URL` (8 GiB cap).
- Override either with `-megammrurl <url>` / `-archiveurl <url>`, or the
  `MINIMA_MEGAMMR_URL` / `MINIMA_ARCHIVE_URL` environment variables.
- Both endpoints advertise `Accept-Ranges` and serve `Content-Length`, so a
  short body is a failed transfer, not a valid prefix. A partial archive is
  rejected (see the raw-import section below).

## Archive retention (`MAX_KEEP_BLOCKS`)

`ArchiveManager` keeps `MAX_KEEP_BLOCKS = 2000 * GeneralParams::NUMBER_DAYS_ARCHIVE`
blocks (default 50 days -> 100,000 blocks) and prunes everything below
`tip - MAX_KEEP_BLOCKS`.

- Pruning only happens when `-archive` is **not** set (`checkForCleanDB()`).
- It runs during sync, not only at shutdown, so the retained range **slides
  with the tip**. A tool that expects block 1 will only ever see the tail.
- If an unpruned archive is required (analytics from genesis, integrity
  checks), start the node with `-archive`.

Consequence: a report of "stops at block N" often means `MIN(block) = tip -
100,000`, not a failure. Check with:

```sh
sqlite3 <archive> "SELECT COUNT(*),MIN(block),MAX(block),MAX(block)-MIN(block) FROM syncblock;"
```

## Analytics indexing

- `minima-analytics -index` is **one-shot**: it indexes the archive range that
  exists at the moment it runs, then exits (or serves). It does not follow a
  live archive; re-run it to catch up.
- It prints the archive range and the indexed height/lag on every pass. Use
  those two lines before assuming a bug.
- Both archive layouts are supported: chunked (`chunkid`/`blockchunks`) and
  legacy per-row `syncdata`. Do not assume either.
- A fresh index populates `magic_snapshots` from the block pass; there is no
  separate backfill on a fresh database.

## Raw archive (`*.raw.dat`) imports

- Truncated or corrupt gzip is rejected with an explicit error
  (`truncated gzip archive stream`, `gzip archive stream error`,
  `raw archive ended early after N of M blocks`). If you see one of these, the
  file or download is short — it is not a parser bug.
- `RawArchiveInput::hasError()` must be checked after an empty final IBD; a
  short archive still ends with an empty chunk.

## Generated files — do not hand-edit

- `COMMAND_REFERENCE.md` is generated from the node capability manifest.
  Regenerate with `tests/gen_command_reference`; `tests/test_docs_manifest`
  asserts it matches.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target minima -j
ctest --test-dir build
```

Directly runnable correctness tests:

```sh
./build/test_consensus_vectors
./build/test_script_conformance
./build/test_serialization
./build/test_archive_records <archive-db> [expected-count]   # expects a full archive from block 1
./build/test_archive_tail <archive-db> <archive.raw.dat>
```

Analytics is a separate, optional target (needs DuckDB headers/lib):

```sh
cmake -S . -B build-analytics -DCMAKE_BUILD_TYPE=Release -DMINIMA_BUILD_ANALYTICS=ON
cmake --build build-analytics --target minima-analytics -j
```

## Consensus-adjacent changes

- Match the Java reference in `../reference/minima-core` rather than inventing
  local semantics; the project's stated goal is byte/behaviour parity.
- `Main::isSyncIBD()` is a transient flag owned by `TxPoWProcessor`. Do not use
  it to decide chain acceptance; use the local chain state instead
  (`n_i_o_message.cpp::isBootstrappingChain()`).
