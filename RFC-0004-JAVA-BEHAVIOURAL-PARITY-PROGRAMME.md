# RFC-0004 — Java Behavioural Parity Programme for Minima More

- **Status:** Draft / Proposed
- **Scope:** Consensus correctness, test infrastructure, release gating —
  **Minima More and Hawk Node** (shared consensus core)
- **Related:** RFC-0001 (storage roles), RFC-0002 (node-mode benchmarking),
  RFC-0003 (engine benchmark harness), `OPTIMIZATION_BACKLOG.md`,
  `PORTABILITY.md`, `reference-bench/README.md`
- **Author:** Minima More / PureMinima C++
- **Date:** 2026-11-07

---

## 1. Summary

Minima More is a hand-port of the Java Minima node. The port is large
(**731 files / ~111k lines** of `org/` C++ against a **440-file / ~85k-line**
Java reference) and is currently validated by a small golden-vector suite:
12 primitives in `tests/test_consensus_vectors.cpp`, ~25 checks in
`tests/test_script_conformance.cpp`, and a serialization test that prints hex for
*manual* comparison with Java. `reference-bench/README.md` explicitly states that
`minima-core` is a **performance baseline, not a byte-parity oracle**.

Two consequences follow:

1. Divergences are found by inspection, one at a time. A comparator inversion in
   `TxPowTree::recalculateTree()` truncated cumulative fork weight since the first
   tracked import (2026-07-21) and was only found by a targeted review in
   November 2026.
2. "No fork can possibly arise" is currently an assertion, not a property we can
   test. Nothing in CI compares Minima More's *behaviour* against the Java node
   on the same inputs.

This RFC defines a **behavioural parity programme**: a differential harness, a
divergence register, a prioritised package audit, and release gates. Success is
defined as *identical accept/reject decisions and identical consensus state for
the same input sequence over a frozen corpus*, not identical source text.

Parity is **necessary for fork-safety but not sufficient for correctness**:
agreement with a buggy oracle is still a bug. §5.6 defines the assurance levels
(robustness, crash consistency, liveness, platform determinism, specification
conformance, security, operations) that sit outside A/B.

### 1.1 Repositories in scope

The programme covers **both** C++ nodes. They share the same hand-ported Minima
consensus core, and the comparator defect existed in both; fixing it in one repo
only would leave the network with two divergent chain-selection rules.

| Surface | Files | Parity requirement |
|---|---|---|
| Identical `org/` files | 515 | covered by one oracle and one corpus |
| Differing `org/` files | 206 | classified per file: **safe deviation** (register + test) or **divergence** (fix) |
| minima-more-only | 10 | out of scope (analytics / benchmarking) |
| hawk-node-only | 103 | **not** Java-parity work; needs its own verification (MASH/MSHS2, lookup, DuckDB/ONNX/ZVEC, MCP, modes) |

Counts are normalised (LF) hashes over `org/**/*.{cpp,hpp}`, computed 2026-11-07
(731 minima-more files, 824 hawk-node files). Largest differing areas:
`system/commands` (59), `system/network` (37), `system/brains` (8),
`database/txpowdb` (6), `objects/mmr` (5), `database/txpowtree` (3),
`database/cascade` (2). Every differing file on a consensus path must appear in
the register with a disposition.

**Three-way matrix.** Every run compares:

- A = minima-more @ HEAD
- B = hawk-node @ HEAD
- J = Java oracle @ pinned commit

A and B must agree with each other *and* with J on shared paths. A≠B is a
divergence even when both differ from J: otherwise the two C++ nodes fork each
other on mainnet.

**Ownership.** The harness is shared infrastructure, not a minima-more feature.
Proposed home: a small standalone `totem-parity` repository (or a submodule)
consumed by both nodes, so fixes and corpus entries land once. Until that
exists, minima-more hosts it and hawk-node pins a commit.

---

## 2. Motivation and evidence

### 2.1 Findings that triggered this RFC

A method-level audit of `org/minima/database/txpowtree/` (three classes,
~1,100 lines) produced the following register entries. Full detail in §6.

| ID | Severity | Summary |
|---|---|---|
| D1 | High | `TxPowTree::setLength()` returns non-ancestor fork blocks as cascade input |
| D2 | Medium | `readDataStream()` orphan-skip + fail-closed semantics differ from Java |
| D3 | Medium | `TxPoWTreeNodeAction` exists twice; the standalone one has a raw-pointer API and is dead |
| D4–D10 | Low/Info | Ownership, copy, formatting and dead-declaration drift |

The weight-ordering defect itself (descending vs ascending sort) is fixed in the
working tree and covered by `tests/test_txpowtree_weight.cpp`, which fails on the
old comparator and passes on the new one. It is included here as the proof that
this class of bug is invisible to the current suite.

### 2.2 Why the current suite cannot catch this class of bug

- Golden vectors cover genesis, addresses, coin IDs and WOTS — not block
  acceptance, not fork choice, not MMR continuity.
- Serialization parity is manual (`tests/test_serialization.cpp` prints hex).
- No test replays a block sequence through both implementations.
- The audit covered ~1% of the port.

### 2.3 What the existing "Java parity" work actually covered

The MASH/MSHS2 Java parity programme lived in the **hawk-node** tree, not in
minima-more (`hawk-node/tests/java/org/minima/tests/mshs2/`). Its layers are:

| Layer | Content | Result |
|---|---|---|
| 0/1 | `MiniData`/`MiniNumber`/`MiniString` wire bytes; WOTS/TreeKey roots | 5/5 |
| 2 | KISSVM/MAST verifier | 10/10 |
| 3 | Wire format + `TxPoWChecker` | 13/13 |

minima-more's equivalents are the 12 golden vectors in
`tests/test_consensus_vectors.cpp`, ~25 checks in `test_script_conformance.cpp`,
and the manually-diffed hex from `test_serialization.cpp`. `reference-bench/`
is performance-only and explicitly disclaims byte parity.

**None of these instantiate `TxPowTree`, call `recalculateTree()`, build a fork,
or compare `getTotalWeight()`.** A grep of `tests/java` for `TxPowTree`,
`recalculateTree` or `getTotalWeight` returns nothing. They validate objects and
single-block acceptance; the defect lives in *accumulation across a fork*, which
requires a tree with a ≥2-deep branch plus a recalculation. That is why the
comparator inversion survived every parity run.

The only harness that ever exercised fork choice was TMMT's partition test
(§8.4). Its strict run was **failing** at the time — the failure was logged as a
"fork-choice/weight-propagation defect candidate" — but TMMT is not wired into
minima-more CI or a release gate, so the signal never blocked anything.

---

## 3. Definitions

**Behavioural parity.** For the same ordered input (genesis + block/tx stream +
config), both nodes must produce, at every step:

- the same accept/reject decision and the same rejection class;
- the same `TxPoWID` / `TxBlockID`;
- the same MMR root and peak set;
- the same tree tip, tree root and cumulative chain work;
- the same cascade tip and cascade length;
- the same archive height.

**Fork.** Any input for which the two implementations disagree on acceptance or on
any consensus-visible output above.

**Safe deviation.** A C++-only behaviour that cannot change accept/reject or
chain state (e.g. atomic local state files, bounded local reads, fail-closed
corruption handling). Safe deviations must be registered with a justification and
a test proving they are observationally equivalent.

**Source parity is a non-goal.** The C++ tree intentionally diverges (wide weight
path, hardening, analytics). Only observable behaviour is contractual.

---

## 4. Goals and non-goals

### Goals

- G1: Detect acceptance/state divergences automatically, per block, with a
  minimised reproducer.
- G2: Gate every change under `org/` on the differential suite.
- G3: Audit the remaining consensus surface in risk order, converting each
  finding into a permanent regression vector.
- G4: Keep the divergence register authoritative; no unregistered deviation from
  Java in consensus paths.
- G5: Make the "no known fork" claim reproducible on demand (one command).
- G6: Keep the two C++ nodes (minima-more and hawk-node) in agreement on every
  shared consensus path, not just each of them against Java.

### Non-goals

- Byte-identical source or binary.
- Performance parity (covered by RFC-0002/0003 and `reference-bench`).
- Wallet/RPC/JSON output parity (consumer-facing, not consensus).
- Replacing the existing unit tests; the differential harness complements them.

---

## 5. Architecture

Two levels, because they have different cost/fidelity trade-offs.

### 5.1 Level A — kernel differential (fast, white-box)

Link the same test driver against:

- C++: `libminima_core.a` (already built by CMake).
- Java: the classes under `/Users/peter.cheek/Downloads/minima` (`org.minima.*`),
  compiled once into a jar.

The driver calls the same validation/state functions in both with identical
serialized inputs:

| Kernel call | C++ entry point | Java entry point |
|---|---|---|
| TxPoW ID + weight | `TxPoW::calculateTXPOWID` | `TxPoW.calculateTXPOWID` |
| TxPoW validation | `TxPoWChecker::checkTxPoW` | `TxPoWChecker.checkTxPoW` |
| TxBlock/MMR apply | `TxPoWTreeNode::constructMMR` | `TxPoWTreeNode.constructMMR` |
| MMR root/proof | `MMR` | `MMR` |
| Script eval | `Contract` / KISSVM | `Contract` |
| MiniNumber ops | `MiniNumber` | `BigDecimal`/`MiniNumber` |

Output: a canonical JSON digest per input (`accepted`, `txpowid`, `mmrroot`,
`weight`, `error`). The comparator diffs the two JSON documents.

This level is deterministic, needs no network or DB, and runs in seconds — it is
the one that belongs in CI on every PR.

### 5.2 Level B — node replay (slow, black-box)

Run two real nodes in a hermetic configuration against the same block stream:

- Input: archive import (`archive action:import`) or a replay RPC.
- Both nodes: no mining, no P2P, fixed clock offset, single-threaded where
  configurable, analytics disabled.
- After each block (or every N blocks), sample the observation contract (§5.3)
  from RPC/logs/state files.
- Stop at the first divergence; persist the prefix plus the offending block as a
  corpus entry.

Level B catches integration divergences that Level A cannot (cascade pruning,
tree persistence, restart behaviour, IBD ordering).

### 5.3 Observation contract

Per block:

```json
{
  "height": 123456,
  "blockid": "0x…",
  "accepted": true,
  "reject_class": null,
  "mmr_root": "0x…",
  "tree_tip": "0x…",
  "tree_root": "0x…",
  "chain_work": "…",
  "cascade_tip": "0x…",
  "cascade_length": 2048,
  "archive_height": 123400
}
```

Every N blocks (and at the end of a run):

- cascade total weight and level layout;
- set digest of wallet-relevant coins (sorted, hashed);
- MegaMMR root (when enabled).

### 5.4 Corpus

| Tier | Contents | Runtime target |
|---|---|---|
| C0 | Genesis + first 1,000 mainnet blocks | < 1 min |
| C1 | Mainnet slices at difficulty retargets, super blocks, known forks/reorgs | < 10 min |
| C2 | Synthetic adversarial: wrong parent, duplicate, two-block fork, non-monotonic height, bad MMR root, truncated body, `MiniNumber` boundary/scale-45, oversized script, negative amount | < 1 min |
| C3 | Deterministic mutation fuzzing (fixed seed), minimised by delta-debugging | nightly |

C2 is the most valuable early on: it is cheap, targets known divergence classes,
and does not depend on archive availability.

### 5.5 Tooling layout

Level B should be a **fork of `TOTEM/TMMT/phase1a`**, not a greenfield harness
(see §8.4). Only the kernel driver and corpus are new.

```
parity/
  README.md                 how to run, prerequisites
  kernel/                   Level A driver (C++ + Java)          [new]
  replay/                   Level B, forked from TMMT/phase1a     [borrowed]
  corpus/                   frozen inputs + expected digests      [new/borrowed]
  register/                 DIVERGENCES.md (machine-readable front-matter)
  scripts/                  build java jar, fetch slices, minimise
```

### 5.6 Assurance levels beyond A and B

Differential testing is a **falsifier**, not a proof. Passing A/B/J means "no
observed divergence from this oracle on this corpus". Four independent questions
remain, and each needs its own programme:

| Level | Question | Method | Existing asset |
|---|---|---|---|
| C — Robustness | Does malformed or hostile input crash, hang or exhaust memory? | ASan/UBSan/TSan builds; fuzz wire, IBD, archive and script parsers; resource caps | `hawk-node/tests/fuzz` (2 fuzzers), `ENABLE_SANITIZERS` (hawk only), TMMT corrupt-bundle tests |
| D — Crash consistency | After SIGKILL or power loss, does the node recover to the same state? | Kill at journal/fsync boundaries; compare restart digests | `TMMT/phase1a/scripts/run_crash_test.py`, hawk atomic save |
| E — Liveness and partition | After partition, deep reorg or eclipse, do nodes converge? | TMMT partition/reorg/sync diagnostics; multi-node soak | `run_partition_test.py`, `run_reorg_test.py`, `run_sync_diagnostic.py` |
| F — Platform determinism | Do GCC and Clang, arm64 and x86, make the same decisions and bytes? | Dual-toolchain build; rerun consensus vectors and A/B replay on each | `PORTABILITY.md`, `Dockerfile.linux-build`, TMMT reproducible-build report |
| G — Specification conformance | Is the oracle itself right? | Protocol-spec review; property tests (fork-choice monotonicity, weight conservation, MMR append-only); triage when A/B/J disagree | Java source, Minima protocol docs, TMMT convergence spec |
| H — Security | Can an attacker steal keys, bypass RPC auth or force a fork? | Threat model, auth audit, deserialization review, supply chain | `SECURITY.md`, `REMEDIATION_PLAN.md` |
| I — Operations | Does it survive 24/7 mainnet conditions and upgrades? | Long-run replay, RSS/disk soak, upgrade and rollback drills | `hawk-update`, `hawk-clone`, `reference-bench` soak scripts |

**Status (2026-11-07).** Level A is implemented as `tests/parity/kernel/`: a C++
kernel digest on both nodes and the pinned Java oracle. The outputs are
byte-identical (digest `7ac470fb…b739ac`, oracle commit `c2a54a2`) for
MiniNumber/MiniData/MiniString wires, coin IDs, `Coin`/`Transaction` bytes and
transaction ID, `TxHeader`, `TxBody`, full `TxPoW` and `TxBlock` bytes,
`txpowid`, block weight, KISSVM parse/success flags, Winternitz key/signature/
verify, MMR proofs and `CoinProof` bytes, and a `TxBlock` carrying parent MMR
peaks, and a signed spend block with input proofs. The kernel corpus is the complete block
object graph. Level B has its first leg: `tests/parity/replay_cross_node.py`
runs one node as producer and the other as consumer over real P2P/IBD, and both
directions accept blocks 1..5 with identical IDs in both repos. Full per-block
accept/reject and state-digest replay remains. Level C is implemented in both repos: four fuzz
targets (`MiniData`, JSON, `MiniNumber`, `TxPoW`), ASan+UBSan builds, and a
portable deterministic driver for toolchains without libFuzzer
(`tests/fuzz/run_fuzz.sh`; ~96k inputs per target, clean). Level D is
implemented as `tests/parity/restart_equality.py` (graceful and SIGKILL modes,
per-height hash comparison, liveness). Level E is implemented as
`tests/parity/fork_choice.py`: a deterministic, iptables-free divergent-branch
test using a test-only mining pause (`MINIMA_TEST_PAUSE_FILE`, honoured only
under `-test`). Level F is implemented as `tests/parity/platform_digest.sh` plus
a CI matrix (`.github/workflows/parity.yml`): macOS/arm64 AppleClang 16,
Linux/arm64 GCC 13.3 and Linux/arm64 Clang 18.1, in both repos, all produce the
same payload digest. Level G is partially implemented: `tests/test_tree_properties.cpp`
checks weight conservation, heaviest-child tip selection, idempotent
recalculation, and `setLength` ancestry/pruning over seeded random trees.
Protocol-spec review and A/B/J disagreement triage remain. Level H is partially
implemented as `tests/parity/security_smoke.py` (RPC auth + input-bound
survival); the full threat-model and auth audit remain. Level I is partially
implemented as `tests/parity/soak.py` (short resource/restart soak); 24/7 and
upgrade/rollback drills remain.

**Hard limits that no amount of A/B testing removes:**

- **Oracle bug propagation.** A, B and J can agree unanimously and still be
  wrong. Only Level G distinguishes *parity* from *correctness*.
- **Coverage.** A finite corpus cannot prove absence of divergence. Every claim
  must be phrased as "no known divergence on corpus X with oracle J at commit
  Y".
- **Unobservable defects.** Memory-safety and lifetime bugs can leave state
  digests identical until they corrupt in production. Level C is the only
  detector.
- **Environment.** Passing on macOS/Clang says little about Linux/GCC or ARM.
  Level F is mandatory given this port's history (`PORTABILITY.md`).

---

## 6. Divergence register (initial)

| ID | Area | Status | Disposition |
|---|---|---|---|
| D1 | `setLength` side branches | **Fixed** | Matches Java (ancestors only); orphan preservation left to the archive (Q4) |
| D2 | `readDataStream` orphan/fail-closed | Partly safe | Register as safe deviation; prove no acceptance change |
| D3 | Duplicate `TxPoWTreeNodeAction` | Open | Delete dead class; keep the `shared_ptr` API |
| D4 | `weak_ptr` parent | Safe | Register; test for lifetime edge cases |
| D5 | `getChildren()` const-ref | Safe | Register |
| D6 | Relevant-coin copies | Safe | Register |
| D7 | TxBlock deep copy | Safe | Register; verify no post-construction mutation |
| D8 | Persisted node count source | Safe | Register |
| D9 | `printTree` date format | Non-consensus | Ignore |
| D10 | Undefined `TxPoWTreeNode::main` | Dead | Delete |
| D11 | hawk-node lookup/watchlist pushes | Hawk-only | Registered extension; must not alter shared coin/tree state (covered by the A=B leg) |
| D12 | hawk-node shutdown hangs in `NIOManager::ThreadPool::awaitTermination()` (unconditional `join`), so `saveAllDB()` never runs and the tree is lost on restart | **Fixed** | Detach stuck workers after the drain deadline; graceful checkpoint verified by `tests/parity/restart_equality.py` |
| D14 | IBD path inserted blocks without `checkTxBlockOnly`; a crafted weight-0 next-height block became the chain tip (reproduced on the Java oracle) | **Fixed (C++)** | `checkTxBlockOnly` plus an `isBlock()` block-PoW gate, a previous-peaks match against the parent MMR, input coin-proof verification against the parent MMR, duplicate-input rejection, a rebuilt-MMR root/total match, and a PoW gate on empty-tree bootstrap in `processSyncBlock`; registered deviation from Java for invalid blocks; valid replay/reorg re-verified |
| D13 | 30-minute IBD limit is keyed by peer endpoint, so a legitimate re-IBD for fork recovery (same NAT/host) is refused | **Open (candidate)** | Configurable via `-ibdlimit`; decide whether to also allow on a new session or chain progress |

New divergences found by the harness are appended here **before** being fixed or
declared safe.

### Residual IBD validation gaps (D14 follow-ups)

The D14 fix now enforces `checkTxBlockOnly`, a valid block PoW, a parent-MMR
previous-peaks match, per-input coin-proof verification, duplicate-input
rejection and a rebuilt-MMR root/total match against the PoW-committed header.
These related gaps remain and are the next regression targets:

1. **Transaction witnesses/signatures** — the IBD payload carries `TxBlock`s
   only, with no transaction bodies, so ownership signatures cannot be checked
   at insertion. Coin existence/spend state and the committed MMR are checked;
   a peer that mines a valid block can still include a spend whose witness is
   never verified by IBD-only peers.
2. **Unsolicited IBD** — a node accepts an IBD without a peer-ahead greeting and
   an outstanding sync request (the Java node does the same).
3. **Empty-tree bootstrap** — the first block on an empty tree now needs a
   valid block PoW, but there is still no trusted genesis/checkpoint, so an
   eclipse attacker can offer a self-mined valid-PoW root.

---

## 7. Workstreams and milestones

### WS0 — Close the txpowtree package (blocking)

- Fix D1; add a forked-tree `setLength` test (assert ancestors only, ascending).
- Delete D3/D10 dead code.
- Run Level A for `TxPoWTreeNode`/`TxPoWTree` paths.
- **Exit:** register D1–D10 closed or registered-safe; package has unit +
  differential coverage.

### WS1 — Harness skeleton

- Fork `TOTEM/TMMT/phase1a` into `parity/replay/`; parameterise A/B binaries and
  make the partition step portable (§8.4).
- Java jar build (`scripts/build_minima_core.sh` already exists; see §9).
- Level A driver + canonical digest + comparator.
- CI job on PRs touching `org/`.
- **Exit:** one C0 slice and one C2 case run green in CI on both nodes, and the
  TMMT shallow partition test passes with the fixed binary.

### WS2 — Package audits in risk order (both repos)

Each package gets: method diff across **minima-more, hawk-node and Java** →
differential coverage → register entry → regression vector.

0. Classify the 206 differing `org/` files (§1.1): safe deviation, divergence,
   or hawk-only. Anything unclassified blocks WS2 sign-off.
1. `MiniNumber` (rounding, scale, `getAsInt` low-32, wide path, 64-digit cap).
2. `TxPoW`/`TxBlock`/`TxHeader` validation and difficulty/weight.
3. MMR / MegaMMR.
4. KISSVM / MAST.
5. IBD, cascade, prune/reorg.
6. Serialization of every object on the wire.

### WS3 — Corpus and oracle expansion

- Add C1 slices per package.
- Promote SDK-derived vectors only after they are re-anchored to the Java node
  (§8).
- Add Level B replay.

### WS4 — Release gate

- `parity/run.sh --tier all` required for any release tag.
- Nightly C3 fuzz; failures open a register entry automatically.

### Milestones

| Milestone | Content | Acceptance |
|---|---|---|
| M0 | WS0 complete | txpowtree tests + differential green; TMMT shallow and strict partition tests pass on the fixed binary |
| M1 | WS1 complete | CI blocks a seeded comparator regression |
| M2 | MiniNumber + TxPoW/Block/Header signed off | C0/C2 green; register current |
| M3 | MMR + KISSVM signed off | C1 slices green |
| M4 | Full corpus + Level B | "no known fork" reproducible in one command |

---

## 8. Reuse of existing assets (and their limits)

### 8.1 `totem-sdk/packages/txpow`

5.5k lines of TypeScript covering the **transaction envelope only**:
`serializeTxHeader/Body/TxPoW`, `computeTxPoWId`, mining, `verifyProofOfWork`,
chain constants. It does not implement block TxPoW, block difficulty, weight,
super parents, MMR continuity, tree/cascade, or KISSVM.

Its parity gate is against the **Totem extension's** serializer plus a handful of
vectors attributed to `TxHeaderTests_*.java` — not against the Java node's
`TxHeader/TxBody/TxPoW.writeDataStream`. The 33 serialization tests mostly assert
structure (field offsets, 36-byte hash framing, determinism), not full byte
vectors produced by Java, and the builder only emits *fresh transaction* TxPoW
(`blockNumber = 0`, `blockDifficulty = MAX_HASH`, zero MMR root). It is therefore
**not an oracle today**, and it cannot generate the block-level inputs where the
fork-choice defect lives.

**Where it is genuinely useful (later, bounded):**

1. **Wire-layout checklist** for the WS2.6 serialization audit — its source
   comments enumerate field order and encoding precisely. Free documentation.
2. **Corpus seeds:** empty transaction/witness, `MAX_HASH` defaults, PRNG
   handling, MiniNumber length-prefix boundaries (128/32768).
3. **A third fuzz target for the tx-envelope layer** — only after its vectors
   are re-produced from the Java node; then Java ↔ C++ ↔ SDK tri-differential
   is cheap.
4. **Consumer interop** — a separate goal: it is what wallets/extensions use to
   build and broadcast transactions, so a Java↔SDK KAT would reduce
   "wallet-produced TxPoW rejected by node" incidents. **This is not node
   consensus parity.**

Do **not** use it to certify the node until the same vectors are produced by the
Java node and the SDK agrees. It is not on the critical path: priority stays
WS0 → WS1 → WS2. The tri-differential envelope KAT is a 1–2 day nice-to-have if
consumer interop becomes a product priority.

### 8.2 `totem-sdk/packages/kissvm` and `packages/core`

KISSVM v1 evaluator (~14.8k lines, Rust/WASM-backed) and core primitives
(`MiniNumber`, MMR, WOTS, wire serializer, ~10.6k lines). They are the most
relevant SDK assets for WS2.3/WS2.4, but carry the same caveat: they are
client-side implementations of the protocol, and their tests do not currently
prove node parity. Treat them as **additional differential targets**, never as
ground truth.

### 8.3 Existing repo assets

- `tests/test_consensus_vectors.cpp`, `tests/test_script_conformance.cpp`,
  `tests/test_serialization.cpp`, `tests/test_mmr.cpp`, `tests/test_h2_import.cpp`.
- `reference-bench/scripts/build_minima_core.sh` (Java build), `run_mainnet_sync.sh`.
- Archive import/export tooling for corpus capture.

### 8.4 TMMT phase 1A — the two-node harness from the partition experiment

`TOTEM/TMMT/phase1a/` already implements most of Level B against **two real
`minima-more` nodes**: private genesis, P2P partition via iptables, competing
miners, reconnect, stable selected-tip agreement, three-validator convergence,
deterministic replay fixtures, crash recovery, a version gate and reproducible
build reports.

It is also the harness behind the diagnostic that motivated this RFC.
`PARTITION.md` records the failing strict run: A's branch at height 15
(`87362.74` work) versus B's at height 17 (`137153.71`), both height-11 children
reported at `41333.33`, A never switching. That is the comparator defect.

**Borrow directly**

| Asset | Use in this RFC |
|---|---|
| `scripts/run_real_integration.py` (`Node`, `wait_until`, `rpc`, port allocation, keystore sharing) | Level B node lifecycle and observation plumbing |
| `scripts/run_shallow_partition_diagnostic.py` | Totem-free fork-switch test; smallest end-to-end check of tree fork choice |
| `scripts/run_partition_test.py` | Full partition → divergent branches → reconnect → selected-tip agreement |
| `scripts/run_sync_diagnostic.py` | Separates transport/IBD from fork choice |
| `reports/parent_shallow_partition.json` | Observation schema: height, hashes, weights, per-stage log counters |
| `scripts/check_minima_version.py` | Gate pattern to pin the exact Java/C++ commit and build stamps |
| `scripts/build_patched_minima.sh` / `build_reference_minima.sh` | Isolated worktree builds (adapt to "C++ HEAD" vs "C++ HEAD+patch" vs "Java oracle") |
| `tools/totem-replay-fixture`, `REPLAY.md` | Six-block + losing-fork deterministic fixture: C1/C2 corpus seed |
| `scripts/verify_reproducible_build.py`, `freeze_parity_report.py` | Report hashing and build provenance |

**Adapt**

- Run **two different binaries** (C++ vs Java) instead of one `MINIMA_NODE`.
  `Node.start()` already takes the shared binary; add `MINIMA_NODE_A/B` or a
  wrapper script.
- Drop Totem-specific steps: `auxblock`, `TOTEM_AUX_*`, `ConsensusStore`.
  Java has no `auxblock`, so use `status`/`block` observations only.
- Replace iptables with a portable partition (`pfctl` on macOS, or a userspace
  TCP proxy) so the harness runs on developer machines as well as Linux CI.
- Relax `check_minima_version.py`'s `Totem-Edge/minima-more` origin assertion.

**Not usable as an oracle.** TMMT compares `minima-more` with itself. It supplies
the Level B plumbing and one excellent acceptance test, not the Java side.

**Forking this harness does not, by itself, add Java parity.** Two separate
deliverables are still required, both in WS1:

1. **Level A (the actual oracle):** a Java driver compiled against the pinned
   `minima.jar` and a C++ driver against `libminima_core.a`, exchanging the same
   serialized inputs and diffing canonical digests.
2. **Level B Java side:** launch the Java node as node B in the forked harness.
   It accepts the same CLI surface (`-data`, `-port`, `-rpcenable`,
   `-rpcpassword`, `-daemon`) and exposes `status`/`block`, so a `java -jar`
   wrapper is enough; Totem-only steps must be removed.

Without both, the fork only proves that the C++ node is self-consistent — the
same blind spot as today.

**Immediate use — first validation of the comparator fix.** The strict
`run_partition_test.py` is the test that currently fails for the weight reason;
it is the one that must flip to pass. The shallow
`run_shallow_partition_diagnostic.py` already passes today (its fork is only one
block deep, where ascending order happens to be sufficient), so treat it as a
non-regression check, not as evidence. Acceptance: A switches to B's heavier
branch (height 17, `137153.71` work) and both nodes settle on one selected tip
across repeated samples. This becomes the M0 acceptance test.

**Prerequisite.** The partition scripts need `iptables` (root or `sudo`) today,
i.e. Linux. On this workstation (macOS) they will fail unless the portable
partition step is added first.

---

## 9. Prerequisites and risks

| Risk | Impact | Mitigation |
|---|---|---|
| No JDK on build hosts (`javac` missing; JRE 1.8 only) | Cannot build the Java oracle | Install a JDK or run the oracle in a container; pin the version |
| Java reference may differ from the deployed network version | False parity | Pin the exact upstream commit in the harness; record it in every report |
| `reference-bench` warns of different address derivation | Wrong assumptions | Never reuse bench clones as the oracle; use the pinned source tree |
| Nondeterminism (time, threads, map order) | Flaky comparisons | Fixed clock offset, deterministic mode, sorted digests, single-thread replay |
| Wide `MiniNumber` path vs Java `BigDecimal` | Boundary mismatches | Explicit boundary corpus (2^64±1, scale 44/45, huge weights) |
| Harness becomes a second product | Maintenance cost | Keep it thin; digests and diff only, no node logic |
| Optimisation work reintroduces divergence | Forks | Gate `OPTIMIZATION_BACKLOG.md` items (esp. #4 MiniNumber fast path, recalc cadence) on the harness |
| Corpus licensing/size | Cannot ship mainnet slices | Store hashes + fetch scripts; keep slices out of git |
| TMMT partition needs `iptables` + root (Linux only) | Harness cannot run on macOS dev machines | Add `pfctl`/TCP-proxy partition mode; run strict tier in Linux CI |
| The oracle itself may be wrong | Unanimous A/B/J agreement on a bug | Level G spec review and property tests; treat A/B/J disagreement as a triage input, not an automatic Java-is-right verdict |
| Corpus coverage is finite | "No known divergence" read as "no divergence" | State the claim with corpus hash + oracle commit in every report and release note |

---

## 10. Acceptance criteria

The programme is complete when:

1. `parity/run.sh --tier c0,c1,c2` passes on a clean checkout with one command
   for the full A/B/J matrix (minima-more, hawk-node, Java).
2. Every consensus package in §7 WS2 has a signed register entry set, and all
   206 differing files are classified.
3. CI fails on any `org/` change that diverges on C0/C2, in either C++ repo.
4. A nightly C3 run has been green for 14 consecutive days.
5. The register contains zero open High/Medium entries, and every safe deviation
   has a test asserting observational equivalence.
6. Release notes for both nodes state the exact Java commit and corpus hash the
   release was validated against.

Until (1)–(6) hold, no release should claim "at parity" or "fork-safe".

**Release assurance is broader than parity.** A release additionally requires:

- Level C: sanitizer build clean and the fuzz corpus green for 14 days;
- Level D: SIGKILL/restart equality at every journal and fsync boundary;
- Level F: GCC and Clang builds agree on consensus vectors and C0 replay;
- Level H: no open Critical/High findings in `SECURITY.md` /
  `REMEDIATION_PLAN.md`.

Levels E, G and I are continuous programmes rather than one-off gates.

---

## 11. Open questions

1. Which upstream Java commit is the canonical oracle (mainnet-deployed vs
   latest `minima-core`)?
2. Level B: drive the Java node via archive import, RPC replay, or a custom
   harness that instantiates `TxPoWProcessor` directly?
3. **Resolved (§1.1):** shared harness in a standalone `totem-parity` repo;
   minima-more hosts it and hawk-node pins a commit until that exists.
4. Is preserving orphan blocks in the archive (D1's apparent intent) worth a
   deliberate, registered extension to Java, or should we match Java exactly?
5. Which hawk-only subsystems need differential verification of their own
   (MASH/MSHS2, lookup, analytics, MCP) rather than Java parity, and who signs
   those off?

---

## 12. Appendix — audit scope completed so far

- `org/minima/database/txpowtree/{tx_pow_tree,tx_po_w_tree_node,tx_po_w_tree_node_action}.{hpp,cpp}`
  compared method-by-method against
  `/Users/peter.cheek/Downloads/minima/database/txpowtree/*.java`.
- Weight ordering fixed and regression-tested (`tests/test_txpowtree_weight.cpp`).
- D1 (`setLength` side branches) fixed to Java semantics and regression-tested
  (`testSetLengthReturnsMainChainOnly`): a side branch below the new root is no
  longer handed to the cascade. Verified the test fails on the old code
  (`removed size=4`, `0x12b` returned) and passes on the fix.
- Weight representation (`MAX_VALDEC / difficulty`, DECIMAL32) compared against a
  7-significant-digit HALF_EVEN emulation on 12 difficulty values: 0 value
  mismatches; format differs (`E+9` vs `e+09`) but parses identically.
- Level C implemented: four fuzz targets + sanitizer builds; `run_fuzz.sh`
  passed ~96k inputs per target with no findings in both repos.
- Level D implemented: `restart_equality.py` passes for both nodes. Graceful
  restart recovers the full checkpoint with identical per-height hashes; SIGKILL
  recovers a consistent prefix and the node resumes mining.
- D12 found and fixed during Level D: `hawk-node` hung in the NIO thread-pool
  join, so shutdown never reached `saveAllDB()`.
- Level E implemented and passing in both nodes: a paused-miner, iptables-free
  fork test (`fork_choice.py`) with a 1-block light branch and a 5-block heavy
  branch. Both A nodes adopted the heavy tip and resumed mining. Required on a
  private network: `-ibdlimit 0` (D13) and `-allowallip true` (hawk SSRF guard).
  The deterministic pause uses the test-only `MINIMA_TEST_PAUSE_FILE` hook.
- Level F demonstrates cross-toolchain agreement on the consensus payload:
  macOS/arm64 AppleClang 16, Linux/arm64 GCC 13.3 and Linux/arm64 Clang 18.1
  hash to `f45c4ca9ecb9e87ddeed07c23c197e6dfc2dda4ef86e7c4946a7837f5c9821f9`
  in both repos. minima-more was missing the Token serialization vector; it was
  ported from hawk-node, which also aligned the digests across repos.
  x86_64 remains a CI-native matrix entry (no local emulation attempted).
- Level G (partial): randomized property tests (200 trees, 100 setLength cases)
  pass in both nodes: P1 node total = own + children, P2 root conservation,
  P3 heaviest-child tip, P4 idempotent recalc, P5 setLength returns ancestors
  only, P6 tip preserved and non-descendants pruned.
- Level B (D14): crafted next-height/orphan/non-monotonic blocks rejected by the
  hardened C++ IBD path in both repos; the Java oracle accepted the crafted tip.
- Level B (D14 capture): a real tip block captured from the node's own IBD and
  replayed with only the nonce corrupted (`isblock=0`) is rejected by
  `invalid block PoW`; a version re-mined to a valid block PoW with the previous
  peaks stripped (`isblock=1`, weight 10000) is rejected by
  `previous peaks mismatch`; a version with the header MMR root rewritten and
  re-mined is rejected by `MMR root mismatch`; and a spend block with one input
  coin proof path bit flipped and re-mined is rejected by `invalid coin proof`
  after its untouched proof was accepted. Frozen tip and tree size are unchanged
  in every mode in both repos.
- Level B (bootstrap): a fresh node with no chain rejects a crafted weight-0
  root and stays at height 0/size 0 in both repos.
- Level B (depth + spends): two C++ nodes replayed 20 blocks containing 3 spend
  blocks and 40 blocks containing 4 spend blocks (crossing the test cascade
  start depth) with identical IDs; a Java-mined spend block replayed Java→C++
  was accepted by the coin-proof and MMR gates in both repos.
- Level B (message type): the captured `pow` and `mmr` corruptions replayed as
  `MSG_TXPOW` were rejected with the tip unchanged (`Failed block check` for the
  block form), so the live message path is not a bypass around the IBD gates.
- Level H (partial): RPC smoke passes in both nodes — unauthenticated, wrong
  password and malformed Authorization are rejected (401), correct auth works,
  a 2 MB body and a 100k-deep command are rejected, and the node stays live.
- Level A: Java kernel oracle operational. The C++ digest matches the pinned
  Java commit `c2a54a2` byte-for-byte on wires, coin IDs, Coin/Transaction/txid,
  TxHeader/TxBody/TxPoW/TxBlock, txpowid, weight, KISSVM flags and MMR root, in
  both nodes.
- Level B (first leg): cross-node IBD in both directions (C++ -> Java and
  Java -> C++) accepts blocks 1..5 with identical IDs in both repos.
- Level B (reorg): cross-implementation partition/reconnect passes in both
  repos; the C++ node adopts Java's heavier 5-block branch.
- Level I (partial): a 45 s soak in both nodes showed chain progress (1 -> 20
  and 1 -> 23), bounded RSS growth (1.4 MB / 1.9 MB) and an exact graceful-restart
  checkpoint (21 -> 21, 23 -> 23).
- Remaining ~99% of the port: not yet audited. That is the subject of this RFC.
