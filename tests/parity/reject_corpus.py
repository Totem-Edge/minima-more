#!/usr/bin/env python3
"""Level B (negative corpus) — malformed-input rejection parity.

Feeds the same deterministic corpus of malformed/truncated wire inputs to the
C++ and Java parsers (`TxPoW::ReadFromStream`, `TxBlock::ReadFromStream`) and
requires identical accept/reject decisions.

Usage:
  python3 tests/parity/reject_corpus.py --cpp build/kernel_digest \
      --core /path/to/minima-core
"""
import argparse
import hashlib
import os
import random
import subprocess
import sys

DEFAULT_CORPUS_SEED = 20261108


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cpp", default="build/kernel_digest")
    ap.add_argument("--core", default=os.environ.get("MINIMA_CORE", ""))
    ap.add_argument("--java-classes",
                    default=os.environ.get("MINIMA_CORE_BUILD", "/tmp/minima-core-build"))
    ap.add_argument("--jdk", default=os.environ.get("JDK", ""))
    args = ap.parse_args()

    if not (os.path.isfile(args.cpp) and os.access(args.cpp, os.X_OK)):
        print(f"FAIL: C++ kernel digest not built: {args.cpp}", file=sys.stderr)
        return 2
    if not args.core or not os.path.isdir(os.path.join(args.core, "src")):
        print("FAIL: set MINIMA_CORE to a minima-core checkout", file=sys.stderr)
        return 2
    javac = os.path.join(args.jdk, "javac") if args.jdk else "javac"
    java = os.path.join(args.jdk, "java") if args.jdk else "java"
    if not os.path.isfile(args.java_classes + "/org/minima/objects/TxPoW.class"):
        print("FAIL: Java classes missing; run tests/parity/kernel/run_kernel.sh first",
              file=sys.stderr)
        return 2

    libs = ":".join(
        os.path.join(args.core, "lib", name)
        for name in sorted(os.listdir(os.path.join(args.core, "lib")))
        if name.endswith(".jar")
    )
    classpath = f"{libs}:{args.java_classes}"
    here = os.path.dirname(os.path.abspath(__file__))
    source = os.path.join(here, "kernel", "KernelOracle.java")
    subprocess.run([javac, "-cp", classpath, "-d", args.java_classes, source],
                   check=True, capture_output=True, text=True)

    # Use the kernel's own valid blocks as controls and truncation seeds.
    probe = subprocess.run([args.cpp], capture_output=True, text=True, check=True)
    valid = {}
    for line in probe.stdout.splitlines():
        if line.startswith("txpow\t"):
            valid["txpow"] = line.split("\t", 1)[1]
        elif line.startswith("txblock\t"):
            valid["txblock"] = line.split("\t", 1)[1]
    if len(valid) != 2:
        print("FAIL: could not extract valid txpow/txblock from kernel digest",
              file=sys.stderr)
        return 2

    corpus = ["", "00", "0000", "ffffffff", "ffffffffffffffff"]
    for name in ("txpow", "txblock"):
        h = valid[name]
        corpus.append(h)
        bits = len(h)
        for frac in (2, 8, 32, bits // 4, bits // 2, 3 * bits // 4, bits - 2):
            n = max(2, min(bits - (bits % 2), frac))
            if n % 2:
                n -= 1
            corpus.append(h[:n])

    rng = random.Random(DEFAULT_CORPUS_SEED)
    for _ in range(24):
        n = rng.randint(1, 96)
        corpus.append(bytes(rng.randrange(256) for _ in range(n)).hex())

    # Deduplicate, preserve order.
    seen = set()
    corpus = [x for x in corpus if not (x in seen or seen.add(x))]

    payload = "\n".join(corpus) + "\n"
    cpp = subprocess.run([args.cpp, "--corpus"], input=payload, text=True,
                         capture_output=True)
    jvm = subprocess.run([java, "-cp", classpath, "KernelOracle", "--corpus"],
                         input=payload, text=True, capture_output=True)

    cpp_lines = cpp.stdout.splitlines()
    java_lines = jvm.stdout.splitlines()
    if len(cpp_lines) != len(corpus) or len(java_lines) != len(corpus):
        print(f"FAIL: output line count cpp={len(cpp_lines)} java={len(java_lines)} "
              f"corpus={len(corpus)}", file=sys.stderr)
        print("cpp stderr:", cpp.stderr[-400:], file=sys.stderr)
        print("java stderr:", jvm.stderr[-400:], file=sys.stderr)
        return 1

    mismatches = []
    for i, (a, b) in enumerate(zip(cpp_lines, java_lines)):
        if a != b:
            mismatches.append((i, a, b))
    if mismatches:
        for i, a, b in mismatches[:8]:
            print(f"  FAIL #{i}\n    cpp : {a}\n    java: {b}", file=sys.stderr)
        return 1

    digest = hashlib.sha256(payload.encode()).hexdigest()
    print(f"corpus: {len(corpus)} inputs")
    print(f"corpus_digest: {digest}")
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
