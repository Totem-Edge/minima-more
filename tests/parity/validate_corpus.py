#!/usr/bin/env python3
"""Level B (semantic corpus) — TxPoWChecker decision parity.

Runs `TxPoWChecker.checkTxPoWSimple` on the same crafted TxPoW cases in C++ and
Java and compares the accept/reject decision:

  A  empty tx/witness, minimum txn difficulty        -> accept
  B  empty tx/witness, maximum txn difficulty        -> reject (work too low)
  C  one input coin, no witness coin proofs          -> reject (proof count)
  D  one input with a proof from the wrong MMR       -> reject (invalid proof)

`ERR` (exception) is normalised to reject; an exception-vs-false difference is
reported as a note, not a failure, because both paths reject.

Usage:
  python3 tests/parity/validate_corpus.py --cpp build/kernel_digest \
      --core /path/to/minima-core
"""
import argparse
import hashlib
import os
import subprocess
import sys


def strip_prefix(lines):
    out = {}
    for line in lines:
        if line.startswith("validate/"):
            key, _, value = line.partition("\t")
            out[key] = value.strip()
    return out


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

    cpp = subprocess.run([args.cpp, "--validate"], text=True, capture_output=True)
    jvm = subprocess.run([java, "-cp", classpath, "KernelOracle", "--validate"],
                         text=True, capture_output=True)

    cpp_cases = strip_prefix(cpp.stdout.splitlines())
    java_cases = strip_prefix(jvm.stdout.splitlines())
    cases = sorted(set(cpp_cases) | set(java_cases))
    if not cases:
        print("FAIL: no validate/ lines produced", file=sys.stderr)
        print("cpp:", cpp.stdout[-400:], cpp.stderr[-400:], file=sys.stderr)
        print("java:", jvm.stdout[-400:], jvm.stderr[-400:], file=sys.stderr)
        return 1

    failures = []
    notes = []
    normalized = []
    for case in cases:
        raw_cpp = cpp_cases.get(case, "MISSING")
        raw_java = java_cases.get(case, "MISSING")
        norm_cpp = "0" if raw_cpp == "ERR" else raw_cpp
        norm_java = "0" if raw_java == "ERR" else raw_java
        normalized.append(f"{case}\t{norm_cpp}")
        print(f"{case}\tcpp={raw_cpp}\tjava={raw_java}")
        if norm_cpp != norm_java:
            failures.append(case)
        elif raw_cpp != raw_java:
            notes.append(case)

    if failures:
        print(f"FAIL: decision mismatch at {failures}", file=sys.stderr)
        return 1
    if notes:
        print(f"note: exception-vs-false at {notes} (both reject)")

    digest = hashlib.sha256(("\n".join(normalized) + "\n").encode()).hexdigest()
    print(f"case_digest: {digest}")
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
