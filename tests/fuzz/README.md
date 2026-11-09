# Fuzz targets (Level C robustness)

Four `LLVMFuzzerTestOneInput` targets covering wire/parse surfaces:

| Target | Surface |
|---|---|
| `fuzz_mini_data` | `MiniData::ReadFromStream` (P2P payloads) |
| `fuzz_json_parser` | JSON parser (RPC/network input) |
| `fuzz_mini_number` | decimal parser + arithmetic/formatting (consensus-critical) |
| `fuzz_txpow` | `TxPoW::ReadFromStream` (IBD / block input) |

## Build

Sanitizers are enabled automatically when fuzz targets are built:

```bash
cmake -S . -B build-fuzz -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DENABLE_SANITIZERS=ON -DMINIMA_BUILD_FUZZERS=ON
cmake --build build-fuzz --target fuzz_mini_data fuzz_json_parser \
      fuzz_mini_number fuzz_txpow -j
```

The build uses libFuzzer when the toolchain ships the runtime
(`-fsanitize=fuzzer`). AppleClang does not, so the targets link
`fuzz_driver.cpp` instead: a deterministic corpus runner/mutator with the same
`-runs=N DIR` interface used by the scripts.

## Run

```bash
ASAN_OPTIONS=detect_leaks=0 tests/fuzz/run_fuzz.sh build-fuzz 2000
```

The script creates a temporary seed corpus (empty, length prefixes, numeric
boundaries, JSON) and runs every target. Exit code is non-zero on any sanitizer
finding or crash.

Expected output on a clean tree:

```
== fuzz_mini_data ==
...
FUZZ PASS
```

## Notes

- `detect_leaks=0` avoids third-party global allocations; crashes and
  ASan/UBSan reports still abort.
- CI should run this nightly with a larger `-runs` value, and on every PR that
  touches parsers, serialization or `MiniNumber`.
