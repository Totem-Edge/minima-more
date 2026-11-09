#!/usr/bin/env python3
"""Level B/E — cross-implementation partition and reorg.

Phase 1: C++ creates a private chain; Java connects and IBDs a common prefix.
Phase 2: both stop (partition). Java mines `--lead` blocks alone; C++ mines one
           competing block alone.
Phase 3: Java restarts (heavy), C++ reconnects and must adopt Java's heavier
           branch via IBD + fork choice.

Asserts the C++ node contains Java's heavy block at the heavy height, then is
live. This is the cross-implementation version of fork_choice.py.

Usage:
  python3 tests/parity/reorg_cross_node.py --cpp build/minima \
      --core /path/to/minima-core [--lead 5]
"""
import argparse
import os
import shutil
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from replay_cross_node import Proc, PASSWORD, free_port, wait_for  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cpp", default=os.environ.get("MINIMA_NODE", "build/minima"))
    ap.add_argument("--core", default=os.environ.get("MINIMA_CORE", ""))
    ap.add_argument("--java-classes",
                    default=os.environ.get("MINIMA_CORE_BUILD", "/tmp/minima-core-build"))
    ap.add_argument("--jdk", default=os.environ.get("JDK", ""))
    ap.add_argument("--common-height", type=int, default=4)
    ap.add_argument("--lead", type=int, default=5)
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("--keep-work", action="store_true")
    args = ap.parse_args()

    if not (os.path.isfile(args.cpp) and os.access(args.cpp, os.X_OK)):
        print(f"FAIL: C++ node not found: {args.cpp}", file=sys.stderr)
        return 2
    if not args.core or not os.path.isdir(os.path.join(args.core, "src")):
        print("FAIL: set MINIMA_CORE to a minima-core checkout", file=sys.stderr)
        return 2
    if not os.path.isfile(os.path.join(args.java_classes, "org/minima/Minima.class")):
        print("FAIL: Java classes missing; run run_kernel.sh first", file=sys.stderr)
        return 2
    jdk = args.jdk or os.path.dirname(shutil.which("javac") or "")
    if not jdk:
        print("FAIL: no JDK (set JDK=/path/to/jdk/bin)", file=sys.stderr)
        return 2

    libs = ":".join(
        os.path.join(args.core, "lib", name)
        for name in sorted(os.listdir(os.path.join(args.core, "lib")))
        if name.endswith(".jar")
    )
    classpath = f"{libs}:{args.java_classes}"

    work = tempfile.mkdtemp(prefix="parity-reorg-")
    cpp_data = os.path.join(work, "cpp")
    java_data = os.path.join(work, "java")
    os.makedirs(cpp_data, exist_ok=True)
    os.makedirs(java_data, exist_ok=True)
    cpp_port = free_port()
    java_port = free_port()

    def cpp_argv(mode, connect=None):
        base = [args.cpp, "-daemon", "-data", cpp_data, "-port", str(cpp_port),
                "-rpcenable", "true", "-rpcpassword", PASSWORD]
        if mode == "solo":
            return [args.cpp, "-solo"] + base[1:]
        argv = [args.cpp, "-test", "-allowallip", "true", "-testchainlength", "512",
                "-ibdlimit", "0"] + base[1:]
        if connect:
            argv += ["-connect", f"127.0.0.1:{connect}"]
        return argv

    def java_argv(mode, connect=None):
        base = [os.path.join(jdk, "java"), "-cp", classpath, "org.minima.Minima",
                "-daemon", "-data", java_data, "-port", str(java_port),
                "-rpcenable", "true", "-rpcpassword", PASSWORD]
        if mode == "solo":
            return base[:4] + ["-solo"] + base[4:]
        argv = base[:4] + ["-test"] + base[4:]
        if connect:
            argv += ["-connect", f"127.0.0.1:{connect}"]
        return argv

    def tip(proc):
        h = proc.height()
        return {"height": h, "hash": proc.block_id(h)}

    cpp = Proc("cpp", cpp_argv("solo"), cpp_data, cpp_port)
    java = Proc("java", java_argv("solo"), java_data, java_port)
    rc = 0
    try:
        print("== phase 1: C++ chain, Java syncs common prefix ==", flush=True)
        cpp.start()
        wait_for(lambda: cpp.height() >= 1, 120, "cpp genesis")
        cpp.stop()

        cpp.argv = cpp_argv("test")
        cpp.start()
        wait_for(lambda: cpp.height() >= args.common_height, args.timeout, "common")
        java.argv = java_argv("test", connect=cpp_port)
        java.start()
        wait_for(lambda: java.height() >= args.common_height, args.timeout, "java common")
        cpp_base = cpp.height()
        java_base = java.height()
        print(f"common: cpp={cpp_base} java={java_base}", flush=True)
        java.stop()
        cpp.stop()

        print(f"== phase 2: partition; Java mines {args.lead}, C++ mines 1 ==", flush=True)
        java.argv = java_argv("test")
        java.start()
        wait_for(lambda: java.height() >= java_base + args.lead, args.timeout,
                 "java heavy branch")
        heavy = tip(java)
        java.stop()
        print(f"java heavy: {heavy['height']} {heavy['hash']}", flush=True)

        cpp.argv = cpp_argv("test")
        cpp.start()
        wait_for(lambda: cpp.height() >= cpp_base + 1, args.timeout, "cpp light branch")
        light = tip(cpp)
        cpp.stop()
        print(f"cpp light:  {light['height']} {light['hash']}", flush=True)
        if heavy["hash"] == light["hash"]:
            print("FAIL: branches did not diverge", file=sys.stderr)
            return 1

        print("== phase 3: reconnect; C++ must adopt Java's heavy branch ==", flush=True)
        java.argv = java_argv("test")
        java.start()
        cpp.argv = cpp_argv("test", connect=java_port)
        cpp.start()

        def cpp_has_heavy():
            if cpp.height() < heavy["height"]:
                return False
            return cpp.block_id(heavy["height"]) == heavy["hash"].lower()

        wait_for(cpp_has_heavy, args.timeout, "cpp adopts java heavy branch")
        print(f"  ok   cpp height={cpp.height()} includes java block {heavy['height']}",
              flush=True)

        rec = cpp.height()
        wait_for(lambda: cpp.height() > rec, args.timeout, "cpp liveness")
        print(f"  ok   cpp advanced past {rec} -> {cpp.height()}", flush=True)

        print("PASS" if rc == 0 else "FAILED")
        return rc
    finally:
        for proc in (java, cpp):
            try:
                proc.stop()
            except Exception as exc:
                print(f"cleanup {proc.name}: {exc}", file=sys.stderr)
        if args.keep_work:
            print(f"work dir kept: {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
