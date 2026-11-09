# Parity / crash-consistency harnesses (Levels A, C-F)

## `kernel/` — Level A (Java oracle)

Byte/hash differential against the pinned Java `minima-core`:

- MiniNumber wire encodings (boundaries, scale 44/45, >2^64 rejection);
- MiniData / MiniString wire;
- coin IDs (`Crypto.hashObjects`);
- `TxHeader` bytes, `txpowid`, block weight;
- `TxBody` bytes and body hash (empty transaction/witness);
- `Coin` input/output bytes and a `Transaction` with state + link hash,
  including the transaction ID;
- full `TxPoW` envelope and `TxBlock` serialization, including a block built
  from a parent MMR (previous-peaks path);
- KISSVM parse/success/exception flags for six scripts;
- Winternitz public key, signature and verification;
- MMR proof and `CoinProof` serialization;
- a signed spend block: TRUE-address coin, input `CoinProof`, and a `TxBlock`
  carrying computed input proofs and new coins;
- MMR root for a fixed leaf sequence;
- PoW difficulty comparisons (accept/reject).

Build the C++ side and run the differential:

```bash
cmake --build build --target kernel_digest
MINIMA_CORE=/path/to/minima-core JDK=/path/to/jdk/bin \
  tests/parity/kernel/run_kernel.sh build
```

The Java node itself can be driven with the same flags (`java_node_smoke.py`):
it starts `org.minima.Minima` with `-solo/-daemon/-data/-port/-rpc*`, answers
`status`, mines and quits. This is the prerequisite for Level B replay.

Verified: digest `7ac470fb…b739ac` against oracle commit
`c2a54a2e46b2c3a4fd541b80e7d7f5e7afe7d4da`, identical for minima-more and
hawk-node. The Java core is compiled with JDK 17 and the pinned `lib/*.jar`.

## `restart_equality.py` — Level D

Starts a solo node, mines, shuts it down, restarts, and compares per-height block
hashes. Two shutdown modes:

- `--shutdown graceful` — RPC `quit`; the node must checkpoint and recover the
  full height and every block hash.
- `--shutdown kill` (default) — `SIGKILL`; the node must recover a *consistent*
  prefix (no corruption, no invented blocks) and become live again. Work since
  the last periodic save is expected to be lost.

```bash
python3 tests/parity/restart_equality.py --node build/minima \
        --checkpoint-height 5 --shutdown graceful
```

Asserts: recovered height never exceeds pre-kill; every recovered block hash
matches; the node mines past the recovered height.

Options: `--settle N` (seconds before SIGKILL, default 12), `--keep-work`.

## `fork_choice.py` — Level E (portable, no iptables)

Deterministic divergent-branch convergence, the iptables-free variant of TMMT's
shallow partition diagnostic:

1. A creates a private genesis and mines a common prefix (`-solo`).
2. Miners are paused (`MINIMA_TEST_PAUSE_FILE`), B syncs the prefix, then both
   are disconnected (`disconnect uid:all`).
3. A mines one block (light branch); B mines `--lead` blocks (heavy branch).
4. Reconnect with miners still paused: A must fetch B's branch and select B's
   tip; then unpause and check liveness.

```bash
python3 tests/parity/fork_choice.py --node build/minima --lead 5
```

Asserts A's block hash at B's height equals B's, and A advances afterwards.

### Test-only mining pause

`MINIMA_TEST_PAUSE_FILE` is honoured only under `-test` params: while the file
exists, the miner sleeps before starting a round. It never affects production
operation; it exists so fork-choice tests are deterministic without killing
nodes or patching them.

### Parameters required on a private network

- `-ibdlimit 0` — the default 30-minute IBD limit is keyed by peer endpoint and
  otherwise refuses a legitimate second IBD (e.g. fork recovery) within the
  window. The harness disables it deliberately.
- `-allowallip true` — `hawk-node` blocks connections to local/private IPs
  (SSRF guard). minima-more accepts it too, so both harnesses pass it.

## `platform_digest.sh` — Level F

Runs the consensus-facing tests and hashes their exact output:

```bash
tests/parity/platform_digest.sh build
```

The `payload_digest` must be identical on every supported platform/compiler.
`.github/workflows/parity.yml` runs the matrix (Linux GCC, Linux Clang,
macOS Clang) and fails unless all digests agree.

Verified locally:

| Repo | Platform | Compiler | payload_digest |
|---|---|---|---|
| minima-more | Darwin/arm64 | AppleClang 16.0.0 | `f45c4ca9…5c9821f9` |
| minima-more | Linux/arm64 | GCC 13.3.0 | `f45c4ca9…5c9821f9` |
| minima-more | Linux/arm64 | Clang 18.1.3 | `f45c4ca9…5c9821f9` |
| hawk-node | Darwin/arm64 | AppleClang 16.0.0 | `f45c4ca9…5c9821f9` |
| hawk-node | Linux/arm64 | GCC 13.3.0 | `f45c4ca9…5c9821f9` |

The repo ships `tests/parity/Dockerfile.linux-matrix` so the Linux legs can be
reproduced locally:

```bash
docker build -f tests/parity/Dockerfile.linux-matrix -t minima-parity-linux .
docker run --rm -v "$PWD":/src -v mm-parity-build:/build -w /src minima-parity-linux \
  bash -lc 'cmake -S /src -B /build -DCMAKE_BUILD_TYPE=Release && \
            cmake --build /build --target test_consensus_vectors test_serialization \
              test_script_conformance test_txpowtree_weight -j && \
            bash /src/tests/parity/platform_digest.sh /build'
```

## `security_smoke.py` — Level H (smoke)

Starts a node and checks the RPC surface:

- no credentials, wrong password and malformed `Authorization` are rejected (401);
- the correct password is accepted;
- a 2 MB body and a 100k-deep `[` command are rejected without crashing;
- the node is still serving after both inputs.

```bash
python3 tests/parity/security_smoke.py --node build/minima
```

This is a smoke test, not the full threat-model/auth audit.

## `soak.py` — Level I (short soak)

Runs a solo node for `--duration` seconds, sampling height, RSS and data size,
then performs a graceful restart and checks the checkpoint.

```bash
python3 tests/parity/soak.py --node build/minima --duration 60
```

Fails on stalled chain progress, RSS growth above `--max-rss-growth-mb`
(default 256 MB), a lost checkpoint after restart, or process death.

## `replay_cross_node.py` — Level B (cross-node IBD)

One node mines a private chain; the other connects over real P2P and IBDs it.
Every block ID for heights 1..H must match on both sides.

```bash
MINIMA_CORE=/path/to/minima-core JDK=/path/to/jdk/bin \
  python3 tests/parity/replay_cross_node.py --cpp build/minima \
  --direction cpp2java --height 5

# and the reverse
... --direction java2cpp --height 5
```

`--direction cpp2cpp` runs two C++ nodes, so depth and spend coverage need no
Java. `--spend-every N` mines a spend into the producer every N blocks:

```bash
# 40 blocks, a spend every 8, crossing the test cascade start depth (32)
python3 tests/parity/replay_cross_node.py --cpp build/minima \
  --direction cpp2cpp --height 40 --spend-every 8 --timeout 900
```

Verified in both repos: C++ accepts Java blocks and Java accepts C++ blocks with
identical IDs for 1..5; `cpp2cpp` accepted 20 blocks with 3 spend blocks and 40
blocks with 4 spend blocks (cascade active) with identical IDs; and a
**Java-mined spend block** replayed Java→C++ was accepted by the new coin-proof
and MMR gates. Full per-block reject/accept and state-digest replay is still to
build.

## `reject_corpus.py` — Level B (negative corpus)

Feeds the same 41 deterministic malformed/truncated wire inputs to
`TxPoW::ReadFromStream` and `TxBlock::ReadFromStream` in C++ and Java and
requires identical accept/reject decisions.

```bash
MINIMA_CORE=/path/to/minima-core JDK=/path/to/jdk/bin \
  python3 tests/parity/reject_corpus.py --cpp build/kernel_digest
```

Verified: corpus digest `58f12c80…`, no decision mismatches in either repo.

## `validate_corpus.py` — Level B (semantic checker)

Runs `TxPoWChecker.checkTxPoWSimple` on the same crafted cases in C++ and Java:

| Case | Input | Decision |
|---|---|---|
| A | empty tx/witness, minimum txn difficulty | accept |
| B | empty tx/witness, maximum txn difficulty | reject (work too low) |
| C | one input coin, no witness proofs | reject (proof count) |
| D | one input with a proof from the wrong MMR | reject (invalid proof) |

```bash
MINIMA_CORE=/path/to/minima-core JDK=/path/to/jdk/bin \
  python3 tests/parity/validate_corpus.py --cpp build/kernel_digest
```

Verified: case digest `6674e884…`, identical decisions. Case D differs in
mechanism (Java throws, C++ returns false); both reject, reported as a note.

## `reorg_cross_node.py` — Level B/E (cross-implementation reorg)

Phase 1: C++ creates a chain; Java IBDs a common prefix. Phase 2: both stop
(partition), Java mines `--lead` blocks, C++ mines one competing block. Phase 3:
Java restarts (heavy) and C++ reconnects; C++ must adopt Java's heavier branch.

```bash
MINIMA_CORE=/path/to/minima-core JDK=/path/to/jdk/bin \
  python3 tests/parity/reorg_cross_node.py --cpp build/minima --lead 5
```

Verified in both repos: C++ (light) adopted Java's heavy tip and resumed mining.

## Adversarial P2P testing

Active adversarial P2P testing (handshake abuse, crafted IBD/TXBLOCK/TXPOW
frames, capture-and-corrupt replay and bootstrap abuse) is maintained in a
private lab environment and is not distributed with this repository. The public
harnesses above stay in-process or protocol-neutral so they can run without the
lab tooling.

The hardening those tests exercise is documented in RFC-0004 (`D14` and the
residual-gap list).

## Findings

- **D12** — `hawk-node` hung on graceful shutdown in
  `NIOManager::ThreadPool::awaitTermination()` (unconditional `join()` after the
  drain deadline), so `saveAllDB()` never ran and a restart lost everything since
  the 30-minute periodic save. Fixed by detaching stuck workers after the
  deadline; verified by `--shutdown graceful`.
- **D13 (candidate)** — the 30-minute IBD limit is keyed by peer endpoint, so a
  node behind NAT (or two nodes on one host) can be refused a re-IBD needed for
  fork recovery. Configurable via `-ibdlimit`; whether the default should also
  consider chain progress / a new session is open.
- **D14** — `processSyncBlock` inserted IBD blocks without `checkTxBlockOnly`.
  A crafted weight-0 next-height block became the tip on both C++ nodes **and**
  the pinned Java oracle. Fixed in C++ with `checkTxBlockOnly`, an `isBlock()`
  block-PoW gate and a previous-peaks match against the parent MMR; valid
  replay/reorg pass. The IBD path now also verifies every input coin proof
  against the parent MMR, rejects duplicate input coins, and rebuilds the block
  MMR to require the header's PoW-committed root/total. Residual, not yet fixed:
  transaction witnesses/signatures cannot be checked from the IBD payload
  (it carries no transaction bodies); unsolicited IBDs are accepted without an
  outstanding sync request; and the empty-tree root is now PoW-gated but still
  has no trusted genesis/checkpoint (an eclipse attacker can offer a self-mined
  valid-PoW root).
