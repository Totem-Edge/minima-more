# PureMinima C++ — Change Report

## Unreleased

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
