#!/usr/bin/env python3
"""Level H — RPC authentication and input-bound smoke test.

Checks that a running node:
  1. rejects RPC without credentials;
  2. rejects a wrong password and a malformed Authorization header;
  3. accepts the correct password;
  4. survives an oversized request body and a deeply nested JSON bomb;
  5. is still live and serving after those inputs.

This is a smoke test, not a full security audit (see SECURITY.md /
REMEDIATION_PLAN.md for the threat model).

Usage:
  python3 tests/parity/security_smoke.py --node build/minima
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
import urllib.error
import urllib.request

PASSWORD = "security-smoke-pass"


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


def raw_rpc(port, command, auth=None, timeout=8):
    req = urllib.request.Request(
        f"http://127.0.0.1:{port}/",
        data=command.encode(),
        headers={"Content-Type": "text/plain"},
    )
    if auth is not None:
        req.add_header("Authorization", auth)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as exc:
        return exc.code, exc.read()
    except Exception:
        return None, b""


def basic(password):
    return "Basic " + base64.b64encode(f"minima:{password}".encode()).decode()


class Node:
    def __init__(self, binary, data, port):
        self.binary = binary
        self.data = data
        self.port = port
        self.rpc_port = port + 4

    def _pids(self):
        try:
            out = subprocess.check_output(
                ["pgrep", "-f", "--", f"-data {self.data}"], text=True)
            return [int(x) for x in out.split()]
        except subprocess.CalledProcessError:
            return []

    def start(self):
        log = open(os.path.join(self.data, "node.log"), "ab")
        subprocess.Popen(
            [self.binary, "-solo", "-daemon", "-data", self.data,
             "-port", str(self.port), "-rpcenable", "true",
             "-rpcpassword", PASSWORD],
            stdout=log, stderr=subprocess.STDOUT,
        )
        end = time.time() + 90
        while time.time() < end:
            status, body = raw_rpc(self.rpc_port, "status", basic(PASSWORD))
            if status == 200 and json.loads(body.decode()).get("status"):
                return
            time.sleep(0.25)
        raise RuntimeError("node did not become ready")

    def alive(self):
        status, body = raw_rpc(self.rpc_port, "status", basic(PASSWORD), timeout=3)
        if status != 200:
            return False
        try:
            return bool(json.loads(body.decode()).get("status"))
        except Exception:
            return False

    def stop(self):
        try:
            raw_rpc(self.rpc_port, "quit", basic(PASSWORD), timeout=3)
        except Exception:
            pass
        end = time.time() + 60
        while time.time() < end and self._pids():
            time.sleep(0.25)
        for pid in self._pids():
            try:
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--node", default=os.environ.get("MINIMA_NODE", "build/minima"))
    ap.add_argument("--keep-work", action="store_true")
    args = ap.parse_args()

    if not (os.path.isfile(args.node) and os.access(args.node, os.X_OK)):
        print(f"FAIL: node binary not found: {args.node}", file=sys.stderr)
        return 1

    work = tempfile.mkdtemp(prefix="security-smoke-")
    node = Node(args.node, work, free_port())
    failures = []

    def check(name, ok, detail=""):
        print(("  ok   " if ok else "  FAIL ") + name + (f"  [{detail}]" if detail else ""))
        if not ok:
            failures.append(name)

    try:
        node.start()

        status, _ = raw_rpc(node.rpc_port, "status")
        check("RPC without credentials rejected", status != 200, f"http={status}")

        status, _ = raw_rpc(node.rpc_port, "status", basic("wrong-password"))
        check("RPC wrong password rejected", status != 200, f"http={status}")

        status, _ = raw_rpc(node.rpc_port, "status", "Basic !!!not-base64!!!")
        check("RPC malformed Authorization rejected", status != 200, f"http={status}")

        status, body = raw_rpc(node.rpc_port, "status", basic(PASSWORD))
        ok = status == 200 and json.loads(body.decode()).get("status") is True
        check("RPC correct password accepted", ok, f"http={status}")

        def rejected(status, body):
            # Minima RPC returns HTTP 200 with {"status": false} for rejected
            # commands; a 5xx/connection failure is also a rejection.
            if status is None or status >= 500:
                return True
            try:
                return json.loads(body.decode()).get("status") is not True
            except Exception:
                return True

        big = "A" * (2 * 1024 * 1024)
        status, body = raw_rpc(node.rpc_port, big, basic(PASSWORD), timeout=15)
        check("oversized body rejected", rejected(status, body), f"http={status}")
        check("node alive after oversized body", node.alive())

        bomb = "[" * 100000
        status, body = raw_rpc(node.rpc_port, bomb, basic(PASSWORD), timeout=15)
        check("deep JSON bomb rejected", rejected(status, body), f"http={status}")
        check("node alive after JSON bomb", node.alive())

        status, body = raw_rpc(node.rpc_port, "status", basic(PASSWORD))
        ok = status == 200 and json.loads(body.decode()).get("status") is True
        check("node still serving after inputs", ok, f"http={status}")

    except Exception as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        failures.append(str(exc))
    finally:
        node.stop()
        if args.keep_work:
            print(f"work dir kept: {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)

    if failures:
        print(f"FAILED: {len(failures)} check(s)")
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
