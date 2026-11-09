#!/usr/bin/env python3
"""Level B prerequisite — launch the pinned Java node with the harness flags.

Proves the Java oracle can be driven the same way as the C++ nodes (`-solo`,
`-daemon`, `-data`, `-port`, `-rpcenable`, `-rpcpassword`), so Level B replay can
run both sides with one orchestrator.

Usage:
  python3 tests/parity/kernel/java_node_smoke.py \
      --core /path/to/minima-core [--java-classes /tmp/minima-core-build]
"""
import argparse
import base64
import json
import os
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request

PASSWORD = "parity-java-node-pass"


def free_port():
    for _ in range(500):
        socks = []
        try:
            s = socket.socket()
            s.bind(("127.0.0.1", 0))
            port = s.getsockname()[1]
            socks.append(s)
            if port + 4 >= 65535:
                continue
            s2 = socket.socket()
            s2.bind(("127.0.0.1", port + 4))
            socks.append(s2)
            return port
        except OSError:
            continue
        finally:
            for sock in socks:
                sock.close()
    raise RuntimeError("no free port pair")


def rpc(port, command, timeout=8):
    req = urllib.request.Request(
        f"http://127.0.0.1:{port}/",
        data=command.encode(),
        headers={"Content-Type": "text/plain"},
    )
    auth = base64.b64encode(f"minima:{PASSWORD}".encode()).decode()
    req.add_header("Authorization", "Basic " + auth)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--core", default=os.environ.get("MINIMA_CORE", ""))
    ap.add_argument("--java-classes",
                    default=os.environ.get("MINIMA_CORE_BUILD", "/tmp/minima-core-build"))
    ap.add_argument("--jdk", default=os.environ.get("JDK", ""))
    ap.add_argument("--timeout", type=int, default=180)
    ap.add_argument("--keep-work", action="store_true")
    args = ap.parse_args()

    if not args.core or not os.path.isdir(os.path.join(args.core, "src")):
        print("FAIL: set MINIMA_CORE to a minima-core checkout", file=sys.stderr)
        return 2
    if not os.path.isfile(os.path.join(args.java_classes, "org/minima/Minima.class")):
        print("FAIL: Java classes missing; run tests/parity/kernel/run_kernel.sh first",
              file=sys.stderr)
        return 2
    jdk = args.jdk
    if not jdk:
        javac = shutil.which("javac")
        if not javac:
            print("FAIL: no JDK (set JDK=/path/to/jdk/bin)", file=sys.stderr)
            return 2
        jdk = os.path.dirname(javac)

    libs = ":".join(
        os.path.join(args.core, "lib", name)
        for name in sorted(os.listdir(os.path.join(args.core, "lib")))
        if name.endswith(".jar")
    )
    classpath = f"{libs}:{args.java_classes}"

    work = tempfile.mkdtemp(prefix="java-node-smoke-")
    port = free_port()

    def pids():
        try:
            out = subprocess.check_output(
                ["pgrep", "-f", "--", f"-data {work}"], text=True)
            return [int(x) for x in out.split()]
        except subprocess.CalledProcessError:
            return []

    def alive():
        try:
            return bool(rpc(port + 4, "status", timeout=2).get("status"))
        except Exception:
            return False

    log = open(os.path.join(work, "node.log"), "ab")
    subprocess.Popen(
        [os.path.join(jdk, "java"), "-cp", classpath, "org.minima.Minima",
         "-solo", "-daemon", "-data", work, "-port", str(port),
         "-rpcenable", "true", "-rpcpassword", PASSWORD],
        stdout=log, stderr=subprocess.STDOUT,
    )

    try:
        end = time.time() + args.timeout
        while time.time() < end and not alive():
            time.sleep(0.5)
        if not alive():
            print("FAIL: Java node did not become ready", file=sys.stderr)
            return 1
        chain = rpc(port + 4, "status")["response"]["chain"]
        print(f"java node ready: height={chain['block']} hash={chain['hash']}")

        end = time.time() + args.timeout
        while time.time() < end and int(chain["block"]) < 2:
            time.sleep(0.5)
            chain = rpc(port + 4, "status")["response"]["chain"]
        if int(chain["block"]) < 2:
            print("FAIL: Java node did not mine", file=sys.stderr)
            return 1
        print(f"java node mining: height={chain['block']}")
        print("PASS")
        return 0
    finally:
        try:
            rpc(port + 4, "quit", timeout=3)
        except Exception:
            pass
        end = time.time() + 90
        while time.time() < end and pids():
            time.sleep(0.25)
        for pid in pids():
            try:
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        log.close()
        if args.keep_work:
            print(f"work dir kept: {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
