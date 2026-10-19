# RFC-0002 — Mainnet Node-Mode Benchmarking (Archive / MegaMMR / Lookup)

- **Status:** Draft / Benchmarking plan
- **Scope:** Load the real bootstrap files; run and measure the node in its
  **Archive**, **MegaMMR**, **Combined** (and optionally **Lookup**) modes on live
  mainnet; compare against the default node and the Java reference.
- **Related:** RFC-0001 (roles/engines), RFC-0003 (engine harness)
- **Depends on:** `reference-bench/` harness, `run_mainnet_sync.sh`, `tcp_proxy.py`

---

## 1. Objective

Measure how the node behaves when it carries **historical data** and **coin proofs**:

- MegaMMR mode: tracks **all coins** (proof generation for any historical coin).
- Archive mode: stores/serves **all TxBlocks** (genesis → tip) for lookup/IBD.
- Combined: both.
- Baseline: a default (pruning) node.

For each: **import/load cost**, **steady-state resources**, and **lookup
latency/correctness** on realistic mainnet data.

---

## 2. Inputs (bootstrap files)

| File | Size | Role |
|---|---|---|
| `https://eurobuddha.com/mega.mmr` | ~502 MB | MegaMMR (all coin proofs) |
| `https://eurobuddha.com/archive.raw.dat` | ~1.5 GB | Archive (all TxBlocks, `RawArchiveInput`) |
| `https://spartacusrex.com/megaarchive/` | daily `minima_megammr_*.mmr` | fresh MegaMMR (≤1 week old) |
| `https://spartacusrex.com/archive/` | archive backups | fresh archive |

Both files are **untrusted input** — the import paths are parsers and must be
hardened (see RFC-0001 §5.4).

---

## 3. Procedure (per node mode)

Confirmed in the C++ source (`megammr action:import`, `mysql action:rawimport|reset`,
`MegaMMR::loadMMR`, `RawArchiveInput`) and matching `docs.minima.global`:

**MegaMMR**
1. Start the node with `-megammr`, unique `-port` (dodges the peers' ~30 min IBD rate
   limit), `-daemon`, RPC enabled, fresh `-data` dir.
2. `megammr action:import file:mega.mmr` → loads MegaMMR, **node shuts itself down**.
3. Restart **without `-clean`**; verify `megammr` info (entries/root) + coin lookups.

**Archive**
1. Start with `-archive true`, unique port, fresh data dir.
2. `mysql action:rawimport file:archive.raw.dat` (import) or `mysql action:reset`
   (import **+ resync**, hours) → node shuts down on completion.
3. Restart without `-clean`; verify block-by-number/id + historical lookups.

**Combined** — perform both imports on one node.

---

## 4. Harness

New `reference-bench/scripts/run_mainnet_meganode_bench.sh` (extends the existing
mainnet bench):

- `MODE=default|megammr|archive|both`; `NODE=cpp|java|both`.
- Unique advertised port per instance.
- **Import phase metrics:** wall time, peak RSS, CPU-seconds, bytes-read.
- **Settle phase (default 300 s):** idle RSS avg/max, CPU-seconds, data-dir size,
  threads.
- **Lookup phase:** scripted query set (see §5), reporting p50/p95 latency + results.
- Post-run: tip block (still synced?), peer count.

Reuse `run_mainnet_commands.sh` for the command surface; add a query set:
`block` by number, `txpow` by id (old + recent), `coins` by address/coinid,
`megammr` info, `balance megammr:true`.

---

## 5. Metrics

| Class | Metrics |
|---|---|
| Import/load | time, peak RSS, CPU-s, I/O bytes |
| Steady-state | idle RSS avg/max, CPU-s/interval, disk, threads |
| Sync | time-to-tip, tip block, peer count |
| Lookup latency | p50/p95 for block/id/coin/proof queries; correctness vs a reference |
| Correctness | coin-set root matches; block hash matches known mainnet |

---

## 6. Phases & gates

1. **P1 C++ only** — default vs megammr vs archive vs both; capture all metrics.
   *Gate:* imports succeed; lookups correct; node stays synced.
2. **P2 Java comparison** — same modes on the Java reference (same host, unique ports).
   *Gate:* comparable results captured.
3. **P3 Report** — `MAINNET_TESTS.md` + `RESULTS_LOG.md` + results dir; charts.

---

## 7. Risks / caveats

- **Archive resync is hours**; MegaMMR import is minutes but RAM-heavy. Schedule.
- **Disk:** A1 has ~42 GB free; archive import may add several GB — monitor.
- **Port/identity:** fresh port per fresh node (peer IBD rate limit).
- **Ported-but-untested** C++ import paths — validate before trusting numbers
  (DB grows, root/hashes correct).
- Single-run, shared-host numbers — indicative; note in the report.

## 8. Open questions

1. Archive: run full `reset` (import+resync) or rawimport only?
2. Include a **Lookup** node mode now, or defer until RFC-0003 picks an engine?
3. Java comparison in P2 — all three modes, or archive only (heaviest)?
