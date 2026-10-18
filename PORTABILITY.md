# Portability: Why macOS-to-Linux/GCC Matters

This project started as a hand-ported C++ reimplementation of the Minima node. The original development happened on macOS with AppleClang. That is a perfectly good compiler, but a production blockchain node ultimately needs to run on Linux servers. This document explains why the GCC/Linux port work is important and how it protects the correctness of the compiled machine code.

---

## 1. A Blockchain Node Is Machine Code

The C++ source is not the final product. The final product is the **native binary** that runs on a CPU. Different compilers (Clang vs. GCC), different standard-library implementations (libc++ on macOS vs. libstdc++ on Linux), and different operating systems all produce different machine code from the same source.

If the node only compiles on one platform, you only have **one proven implementation**. Bugs that are hidden by that toolchain’s quirks become part of the binary. Porting to a second toolchain is one of the cheapest ways to surface those bugs before they affect mainnet.

---

## 2. Why Linux / GCC Specifically?

- **Server deployment target.** Almost all cloud and bare-metal blockchain infrastructure runs Linux. A node that cannot build on Linux cannot be deployed at scale.
- **GCC is the default Linux compiler.** Many distributions ship GCC as the system compiler. If the code builds only with Clang, operators have to install a non-default toolchain, which complicates packaging and reproducibility.
- **Different diagnostics.** Clang and GCC catch different classes of errors. A program that is “warning-free” on Clang can still have undefined behaviour or missing includes that GCC rejects. Building on both compilers is a form of double-entry bookkeeping for the source code.
- **Different standard-library header graphs.** macOS libc++ and Linux libstdc++ include transitive headers in different orders. Code that accidentally relies on a header being pulled in transitively will fail on one platform but not the other. This is exactly what happened with `n_i_o_message.hpp` and `<vector>`.

---

## 3. What the Linux/GCC Work Actually Did

The practical deliverables were:

### 3.1 Fixed a missing include

`org/minima/system/network/minima/n_i_o_message.hpp` declared `std::vector<MiniData>` static members without `#include <vector>`. AppleClang pulled `<vector>` in through another header; GCC 13 refused to compile it.

This is a classic portability bug: the code was never technically correct, but one toolchain was lenient.

### 3.2 Added `override` everywhere

GCC (and Clang with `-Werror=suggest-override`) flags virtual functions that override a base-class method but are not marked `override`. This catches signature mismatches that silently introduce slicing or incorrect dispatch. In a consensus system, incorrect virtual dispatch can mean the wrong command handler or the wrong KISSVM value type is executed.

### 3.3 Created a reproducible Linux build environment

`Dockerfile.linux-build` is an Ubuntu 24.04 image with GCC 13 and the required dependencies. This means:

- Any developer can reproduce the exact same Linux build.
- CI can build the binary deterministically.
- The compiled machine code is no longer tied to one developer’s MacBook toolchain.

### 3.4 Verified the full node and tests on Linux

- `minima` executable links successfully under GCC.
- `test_winternitz`, `test_serialization`, and `test_mmr` all pass inside the Linux container.

---

## 4. Why This Protects Consensus

Consensus is a property of the **runtime behaviour** of the binary, not the source text. If the binary behaves differently on Linux than on macOS, the network forks. The portability work reduces that risk in two ways:

1. **Catches language-level bugs.** Missing includes, incorrect overrides, and undefined behaviour can produce different results on different compilers. Fixing them on both toolchains removes a class of cross-platform divergence.
2. **Removes toolchain monoculture.** A single-compiler codebase is fragile. If AppleClang ever changes its default behaviour or a future macOS update breaks the build, the project is stuck. Having a working GCC/Linux path is insurance.

---

## 5. The Bigger Picture: Machine-Code Implementations

When people talk about “machine-code implementations” of a blockchain, they usually mean:

- Compiled native nodes (like this one) rather than JVM or interpreted implementations.
- Reproducible, auditable binaries.
- Deployments that can run on commodity server hardware without a specific vendor stack.

For a machine-code implementation to be trustworthy, the build must be:

- **Portable:** works on more than one OS/compiler combination.
- **Reproducible:** the same source produces the same binary.
- **Tested:** the test suite passes on every supported platform.

The Linux/GCC port gets us from “works on my Mac” to “portable and tested on the platform it will actually run on in production.”

---

## 6. Known Limitations

- The current Docker image is **aarch64** because the host is Apple Silicon. To produce an `x86_64` Linux binary we would need either a multi-arch/qemu build or a Rosetta-enabled x86_64 container. The source-level fixes are identical either way.
- `libzip` and MySQL Connector/C++ are disabled in the Linux container because the Ubuntu `libzip-dev` cmake config is currently broken in this image and MySQL is optional. Core node functionality is unaffected.

---

## 7. Files Added / Changed for Portability

| File | Purpose |
|------|---------|
| `Dockerfile.linux-build` | Reproducible Ubuntu 24.04 / GCC 13 build image |
| `org/minima/system/network/minima/n_i_o_message.hpp` | Added missing `#include <vector>` |
| `org/minima/system/commands/*/*.hpp` | Added `override` to command classes |
| `org/minima/kissvm/values/*.hpp` | Added `override` to KISSVM value classes |
| `build-linux-x86_64-toolchain.cmake` | Experimental Zig cross-compilation toolchain file (kept for reference) |

---

## 8. How to Reproduce

```bash
# macOS build
mkdir -p build && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(sysctl -n hw.ncpu)

# Linux build (Docker)
docker build -f Dockerfile.linux-build -t pureminima-linux-build .
docker run --rm -v "$(pwd):/src" -w /src pureminima-linux-build \
    cmake -S . -B build-linux-docker -DCMAKE_BUILD_TYPE=Release
docker run --rm -v "$(pwd):/src" -w /src pureminima-linux-build \
    cmake --build build-linux-docker -j$(nproc)
```

---

## 9. Recommendation

Keep the Linux/GCC build as a first-class citizen. Before any major change is considered complete, it should build and pass tests on both macOS Clang and Linux GCC. This is the cheapest insurance policy against consensus-diverging machine-code differences.
