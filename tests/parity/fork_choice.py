#!/usr/bin/env python3
"""Level E (portable) — divergent branches converge on the heavier one.

This is the iptables-free variant of TMMT's shallow partition diagnostic. The
partition is created by stopping nodes and mining in isolation; reconnection
uses Minima's own P2P ``-connect``/IBD path.

Scenario:
  1. A creates a private genesis (-solo) and stops.
  2. A (-test) and B (-test -connect A) sync/mine to a common height.
  3. Both stop. B mines alone for `--lead` blocks (the heavy branch).
  4. A restarts (light branch) and B reconnects to it.
  5. A must fetch B's branch and select B's tip.

Usage:
  python3 tests/parity/fork_choice.py --node build/minima
"""
import argparse
import base64
import pathlib
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

PASSWORD = "parity-fork-pass"


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


def wait_for(fn, timeout, what, interval=0.25):
    end = time.time() + timeout
    last = None
    while time.time() < end:
        try:
            value = fn()
            if value:
                return value
        except Exception as exc:
            last = str(exc)
        time.sleep(interval)
    raise RuntimeError(f"timeout waiting for {what}" + (f" (last: {last})" if last else ""))


def connections(node):
    try:
        resp = rpc(node.rpc_port, "network", timeout=5)["response"]
        return sum(1 for c in resp.get("connections", [])
                   if c.get("isconnected") and c.get("valid"))
    except Exception:
        return 0


class Node:
    def __init__(self, name, binary, data, port):
        self.name = name
        self.binary = binary
        self.data = data
        self.port = port
        self.rpc_port = port + 4
        self.pause_file = os.path.join(data, "mining.pause")
        self.log = None

    def _pids(self):
        try:
            out = subprocess.check_output(
                ["pgrep", "-f", "--", f"-data {self.data}"], text=True)
            return [int(x) for x in out.split()]
        except subprocess.CalledProcessError:
            return []

    def _alive(self):
        try:
            return bool(rpc(self.rpc_port, "status", timeout=2).get("status"))
        except Exception:
            return False

    def start(self, extra):
        # Kill any stale daemon left by a previous phase before starting.
        stale = self._pids()
        for pid in stale:
            try:
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        if stale:
            wait_for(lambda: not self._pids(), 15, f"{self.name} stale cleanup")
        self.log = open(os.path.join(self.data, "node.log"), "ab")
        env = os.environ.copy()
        env["MINIMA_TEST_PAUSE_FILE"] = self.pause_file
        subprocess.Popen(
            [self.binary, "-daemon", "-data", self.data, "-port", str(self.port),
             "-rpcenable", "true", "-rpcpassword", PASSWORD] + extra,
            stdout=self.log, stderr=subprocess.STDOUT, env=env,
        )
        wait_for(self._alive, 90, f"{self.name} rpc")
        return self

    def pause(self):
        pathlib.Path(self.pause_file).write_text("paused\n")

    def unpause(self):
        try:
            os.unlink(self.pause_file)
        except FileNotFoundError:
            pass

    def height(self):
        return int(rpc(self.rpc_port, "status")["response"]["chain"]["block"])

    def tip(self):
        chain = rpc(self.rpc_port, "status")["response"]["chain"]
        return {"height": int(chain["block"]), "hash": chain["hash"],
                "weight": chain["weight"]}

    def block_hash(self, height):
        return rpc(self.rpc_port, f"txpow block:{height}")["response"]["txpowid"]

    def stop(self):
        if self._alive():
            try:
                rpc(self.rpc_port, "quit", timeout=3)
            except Exception:
                pass
        try:
            wait_for(lambda: not self._pids(), 120, f"{self.name} shutdown")
        except RuntimeError:
            for pid in self._pids():
                try:
                    os.kill(pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            raise
        if self.log:
            self.log.close()
            self.log = None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--node", default=os.environ.get("MINIMA_NODE", "build/minima"))
    ap.add_argument("--common-height", type=int, default=3)
    ap.add_argument("--lead", type=int, default=5)
    ap.add_argument("--timeout", type=int, default=180)
    ap.add_argument("--keep-work", action="store_true")
    args = ap.parse_args()

    if not (os.path.isfile(args.node) and os.access(args.node, os.X_OK)):
        print(f"FAIL: node binary not found: {args.node}", file=sys.stderr)
        return 1

    work = tempfile.mkdtemp(prefix="parity-fork-")
    while True:
        a = Node("a", args.node, os.path.join(work, "a"), free_port())
        b = Node("b", args.node, os.path.join(work, "b"), free_port())
        if len({a.port, a.rpc_port, b.port, b.rpc_port}) == 4:
            break
    os.makedirs(a.data, exist_ok=True)
    os.makedirs(b.data, exist_ok=True)
    rc = 0
    try:
        print("== genesis: A solo ==", flush=True)
        a.start(["-solo"])
        wait_for(lambda: a.height() >= args.common_height, 90, "genesis chain")
        common_height = a.height()
        a.stop()

        print("== B syncs the common prefix (miners paused) ==", flush=True)
        a.pause()
        b.pause()
        a.start(["-test", "-allowallip", "true", "-testchainlength", "512", "-ibdlimit", "0"])
        b.start(["-test", "-allowallip", "true", "-testchainlength", "512", "-ibdlimit", "0", "-connect", f"127.0.0.1:{a.port}"])
        wait_for(lambda: b.height() >= common_height, args.timeout, "B sync")

        print("== partition ==", flush=True)
        for node in (a, b):
            try:
                rpc(node.rpc_port, "disconnect uid:all", timeout=5)
            except Exception:
                pass
        wait_for(lambda: connections(a) == 0 and connections(b) == 0,
                 45, "partition")
        time.sleep(1)

        print("== light branch: A mines one block ==", flush=True)
        a.unpause()
        wait_for(lambda: a.height() >= common_height + 1, args.timeout, "A block")
        a.pause()
        time.sleep(1.5)
        light = a.tip()

        print(f"== heavy branch: B mines {args.lead} blocks ==", flush=True)
        b.unpause()
        b_base = b.height()
        wait_for(lambda: b.height() >= b_base + args.lead, args.timeout, "B lead")
        b.pause()
        time.sleep(1.5)
        heavy = b.tip()
        print(f"A light={light['height']} {light['hash']}", flush=True)
        print(f"B heavy={heavy['height']} {heavy['hash']}", flush=True)
        if heavy["hash"] == light["hash"]:
            print("FAIL: branches identical", file=sys.stderr)
            return 1

        print("== reconnect (miners paused) ==", flush=True)
        for host, target in ((a, b.port), (b, a.port)):
            try:
                rpc(host.rpc_port, f"connect host:127.0.0.1:{target}", timeout=5)
            except Exception as exc:
                print(f"note: connect {host.name}: {exc}", file=sys.stderr)

        def a_has_heavy_tip():
            if a.height() < heavy["height"]:
                return False
            return a.block_hash(heavy["height"]).lower() == heavy["hash"].lower()

        wait_for(a_has_heavy_tip, args.timeout, "A adopts B's heavy branch")
        print(f"  ok   A height={a.height()} includes B's block {heavy['height']}", flush=True)

        print("== unpause: liveness ==", flush=True)
        rec = a.height()
        a.unpause()
        b.unpause()
        wait_for(lambda: a.height() > rec, args.timeout, "A liveness")
        print(f"  ok   A advanced past {rec} -> {a.height()}", flush=True)
        print("PASS" if rc == 0 else "FAILED")
        return rc
    finally:
        for node in (a, b):
            try:
                node.stop()
            except Exception as exc:
                print(f"cleanup {node.name}: {exc}", file=sys.stderr)
                for pid in node._pids():
                    try:
                        os.kill(pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
        if args.keep_work:
            print(f"work dir kept: {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
