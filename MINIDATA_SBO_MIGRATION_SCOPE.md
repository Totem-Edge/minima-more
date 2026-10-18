# Scope: MiniData small-buffer (inline) storage migration

Status: **IMPLEMENTED (option B).** Result: A1 MMR framing **762 → 698 B/entry
(−8.4%)**; cumulative **858 → 698 B/entry (−18.6%)**, byte-identical, all tests
+ node smoke green. See `reference-bench/RESULTS_LOG.md` (step 4). The plan below
is retained as the record of how it was scoped and executed.

Goal: remove the per-`MiniData` heap allocation so the MMR (UTXO set) framing
gets smaller. This was the last identified MMR-RAM framing lever after steps 1–2
(858 → 762 B/entry).

---

## 1. Why, and the honest payoff

Measured (per 1M objects, M1): `MiniData` ≈ 70 B/obj vs a plain
`std::array<uint8_t,32>` control ≈ 31 B → ~**39 B/obj** is the vector payload +
allocator overhead. Each MMR entry holds ~1 `MiniData` (the hash), so removing
that alloc is worth **~39 B/entry minus the inline-buffer growth** — realistically
**~20–40 B/entry (~3–5% of 762)**.

At 1M coins that is ~30–40 MB out of ~760 MB. **Real but modest.** The effort and
blast radius are non-trivial (below). Recommendation is in §7.

> Bigger strategic lever, for comparison: reducing the *number* of stored MMR
> entries (pruning spent/state-only coins, MegaMMR offload) can save multiples of
> this. Framing is a constant factor; entry count is the dominant variable.

---

## 2. Constraint that shapes the design

`MiniData::getBytes()` returns `const std::vector<std::uint8_t>&`. To make
`MiniData` store bytes inline, `getBytes()` can no longer return a real
`std::vector`. The project is **C++17** (no `std::span`).

Two options:

- **A. Full view migration** — introduce a `ByteView`, change `getBytes()` to
  return it, and rewrite every call site. ~98 sites / 52 files; high churn.
- **B. SBO `MiniBuffer` + implicit `std::vector` conversion (recommended)** —
  `MiniData` stores a small-buffer container; `getBytes()` returns
  `const MiniBuffer&`; `MiniBuffer` exposes the vector-like read API *and* an
  implicit `operator std::vector<uint8_t>() const`. Most call sites compile
  unchanged; only the rare site that needs a mutable vector pays a copy.

---

## 3. Call-site census (current tree)

| Category | Count | Migration impact |
|---|---|---|
| Total `getBytes()` occurrences | 98 (52 files) | — |
| `const std::vector<uint8_t>& x = …getBytes()` | 29 | absorbed by implicit conversion (copy) |
| `std::vector<uint8_t> x = …getBytes()` | 12 | absorbed (copy) |
| passed into a `const std::vector<uint8_t>&` param | 4 | absorbed (copy) |
| `getBytes().data()` / `.size()` direct | 4 | works on `MiniBuffer` |
| `auto&` / `auto` / range-for / indexing | ~49 | works on `MiniBuffer` (read API) |
| **Manual edits expected** | **≈0–5** | only genuinely-vector-mutating sites |

`MiniData` internals (`mini_data.cpp`) using `mData` directly:
`size`, `begin`, `end`, `empty`, `assign`, `data`, `push_back`, `erase`
(plus `stripLeadingZeros`/`compareMagnitude` taking `const std::vector&`). These
are the real adaptation surface — **one file**.

---

## 4. Proposed `MiniBuffer` (C++17)

```cpp
class MiniBuffer {                 // value semantics, SBO
    std::uint32_t mLen = 0;
    union { std::uint8_t mInline[32]; std::uint8_t* mHeap; };
    // ctor/copy/move/dtor; heap only when len > 32
public:
    // read: data(), size(), empty(), begin()/end(), operator[], back()
    // internal mutation used by MiniData: assign/resize/push_back/erase/clear
    // compatibility:
    operator std::vector<std::uint8_t>() const;   // materialise on demand
    // comparisons as needed by MiniData internals
};
```

**Inline capacity = 32** is the sweet spot: hashes/addresses/CoinIDs are 32 B.
Inline 64 would double the struct and cancel the saving.

**Rough size:** MiniBuffer ~40 B; `MiniData` ~= vtable(8) + MiniBuffer(40) +
optionally drop `mDataValue`(8) ≈ 48–56 B (vs 40 today). Net per MMR entry ≈
`+8..16 B contiguous − 39 B heap ≈ −23..−31 B`, i.e. **762 → ~730–740 B/entry**
for the benchmark, before the real (varied) workload.

Also consider removing `MiniData::mDataValue` (the lazy `unique_ptr<cpp_int>`
cache): it adds 8 B and a potential heap alloc; recompute on demand instead.

---

## 5. Staged plan (each stage gated, reversible)

- **P0 — Baseline.** Record current A1 MMR curve (762 B/entry) and capture the
  full gate baseline: `test_mmr`, `test_consensus_vectors`, `test_mempool_index`,
  `test_winternitz`, node build + genesis smoke.
- **P1 — Add `MiniBuffer` + tests.** Unit test equivalence vs `std::vector` for
  lengths 0..200 (content, copy/move, conversion), and O(1) heap use for ≤32 B.
  No call-site changes. Gate: new unit test only.
- **P2 — Adapt `mini_data.cpp` internals** to `MiniBuffer`; keep `getBytes()`
  returning `const std::vector&` by *converting* (so external code is untouched).
  Gate: full suite + byte-identical serialization KATs. This proves the storage
  swap alone is correct.
- **P3 — Switch `getBytes()` to `const MiniBuffer&`; fix residual compile
  errors.** Expect ≤5 manual fixes. Gate: full suite + `test_consensus_vectors`
  12/12 + MMR serialization round-trip + node genesis smoke.
- **P4 — Re-measure** the A1 MMR curve and a real node RSS; record before/after.
- **P5 — (optional)** remove `mDataValue` cache; re-gate.

---

## 6. Risk register

| Risk | Mitigation |
|---|---|
| Serialization/consensus divergence | `MiniData` byte format unchanged; `test_consensus_vectors` + MMR round-trip + Java/consensus KATs as gates |
| Implicit-conversion copies surprise perf | Keep the conversion for compatibility only; audit the 29+12+4 sites; add `const MiniBuffer&` overloads where hot |
| Ambiguous overloads (`==`, ctor) | Prefer explicit `MiniBuffer` overloads; compile-time only, caught immediately |
| ABI/struct growth backfires | Measure at P4; revert P3 if net-negative (P1/P2 are independently safe) |
| Thread-safety of any lazy cache | Drop `mDataValue` or make it immutable-on-construction |
| Blast radius | P2 keeps the public API stable, isolating risk; P3 is the only wide compile churn |

---

## 7. Recommendation

- **ROI is modest** (~3–5% MMR RAM) for a **codebase-wide type change** touching
  hashing, serialization, and wallet objects. Do it only if RAM-on-device at
  mainnet UTXO scale is a hard requirement.
- If pursued, follow P1→P2 first: they add the SBO type and swap the storage
  behind the *existing* API, proving correctness with near-zero call-site churn.
  Decide on P3 (the public-API switch) only if P2 measures a worthwhile win.
- **Higher-leverage alternative to consider first:** reduce MMR *entry count*
  (prune spent/state-only coins; MegaMMR offload) — that scales the whole
  ~762 B/entry down rather than shaving bytes off the framing.

---

## 8. Rough effort

| Stage | Effort |
|---|---|
| P1 (MiniBuffer + tests) | ~0.5–1 day |
| P2 (mini_data internals, API-stable) | ~0.5–1 day |
| P3 (public API switch + fixups) | ~1–2 days |
| P4/P5 (measure + optional cache removal) | ~0.5 day |
| **Total** | **~2.5–4.5 days** + review |

Expected outcome if successful: MMR framing ~762 → ~730–740 B/entry in the
microbenchmark, byte-identical, with the same tests green; real-node saving
≈ (UTXO count) × ~25 B.
