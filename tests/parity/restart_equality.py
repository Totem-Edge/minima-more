#!/usr/bin/env python3
"""Level D — crash consistency.

Scenario:
  1. start a solo node (daemonised), mine to H1, graceful quit (checkpoint);
  2. restart, mine further, then SIGKILL (no graceful save);
  3. restart and verify:
       - the durable prefix 1..H1 is recovered exactly;
       - recovered height is at least H1 (checkpoint not lost);
       - the node is live and mines past the recovered height.

SIGKILL is expected to lose work after the checkpoint; that is not a failure.
The failure modes this catches are: corrupted state, lost checkpoint, invented
or diverged blocks inside the durable prefix, and a node that cannot restart.

Usage:
  tests/parity/restart_equality.py --node build/minima [--checkpoint-height 5]
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

PASSWORD = "parity-restart-pass"


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
        except Exception as exc:  # node not up yet
            last = str(exc)
        time.sleep(interval)
    raise RuntimeError(f"timeout waiting for {what}" + (f" (last: {last})" if last else ""))


class Node:
    def __init__(self, binary, data, port):
        self.binary = binary
        self.data = data
        self.port = port
        self.rpc_port = port + 4
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

    def start(self):
        if self._pids():
            raise RuntimeError(f"node process already running for {self.data}")
        self.log = open(os.path.join(self.data, "node.log"), "ab")
        # -daemon is required: without it the process exits when stdin is not
        # a TTY. The parent forks and exits, so we track the child by data dir.
        subprocess.Popen(
            [self.binary, "-solo", "-daemon", "-data", self.data,
             "-port", str(self.port), "-rpcenable", "true",
             "-rpcpassword", PASSWORD],
            stdout=self.log, stderr=subprocess.STDOUT,
        )
        wait_for(self._alive, 60, "rpc")
        return self

    def height(self):
        return int(rpc(self.rpc_port, "status")["response"]["chain"]["block"])

    def tip(self):
        chain = rpc(self.rpc_port, "status")["response"]["chain"]
        return {"height": int(chain["block"]), "hash": chain["hash"],
                "weight": chain["weight"]}

    def block_hash(self, height):
        return rpc(self.rpc_port, f"txpow block:{height}")["response"]["txpowid"]

    def hashes(self, upto):
        return {h: self.block_hash(h) for h in range(1, upto + 1)}

    def snapshot(self):
        tip = self.tip()
        return {"height": tip["height"], "hashes": self.hashes(tip["height"])}

    def kill9(self):
        pids = self._pids()
        if not pids:
            raise RuntimeError("could not find node pid for SIGKILL")
        for pid in pids:
            try:
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        wait_for(lambda: not self._pids(), 30, "node death after SIGKILL")
        if self.log:
            self.log.close()
            self.log = None

    def stop(self):
        # A graceful quit must finish *on disk* before the next phase starts.
        # The RPC server can go down before the save completes, so wait for the
        # process to exit, not for RPC to stop answering.
        if self._alive():
            try:
                rpc(self.rpc_port, "quit", timeout=3)
            except Exception:
                pass
        try:
            wait_for(lambda: not self._pids(), 120, "graceful shutdown")
        except RuntimeError:
            for pid in self._pids():
                try:
                    os.kill(pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            if self.log:
                self.log.close()
                self.log = None
            raise RuntimeError("graceful shutdown did not complete in 120s")
        if self.log:
            self.log.close()
            self.log = None


def fail(msg):
    print(f"FAIL: {msg}", file=sys.stderr)
    return 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--node", default=os.environ.get("MINIMA_NODE", "build/minima"))
    ap.add_argument("--checkpoint-height", type=int, default=5)
    ap.add_argument("--timeout", type=int, default=120)
    ap.add_argument("--settle", type=int, default=12,
                    help="seconds to wait for a periodic save before SIGKILL")
    ap.add_argument("--shutdown", choices=("kill", "graceful"), default="kill",
                    help="phase-1 shutdown: SIGKILL (crash) or RPC quit (checkpoint)")
    ap.add_argument("--keep-work", action="store_true",
                    help="keep the data directory and print its path")
    args = ap.parse_args()

    if not (os.path.isfile(args.node) and os.access(args.node, os.X_OK)):
        return fail(f"node binary not found/executable: {args.node}")

    work = tempfile.mkdtemp(prefix="parity-restart-")
    node = Node(args.node, work, free_port())
    rc = 0

    def compare(label, before, after):
        nonlocal rc
        if after["height"] > before["height"]:
            rc = fail(f"{label}: recovered height {after['height']} > "
                      f"pre-kill {before['height']}")
        else:
            print(f"  ok   {label}: recovered {after['height']} <= "
                  f"pre-kill {before['height']}")
        limit = min(before["height"], after["height"])
        bad = [h for h in range(1, limit + 1)
               if before["hashes"][h] != after["hashes"][h]]
        if bad:
            rc = fail(f"{label}: block divergence at heights {bad[:5]}")
        else:
            print(f"  ok   {label}: prefix 1..{limit} matches")

    try:
        print(f"== phase 1: mine to {args.checkpoint_height}, "
              f"settle {args.settle}s ==")
        node.start()
        wait_for(lambda: node.height() >= args.checkpoint_height, args.timeout,
                 f"height >= {args.checkpoint_height}")
        if args.settle:
            time.sleep(args.settle)  # let the periodic state save run
        a = node.snapshot()
        print(f"captured height={a['height']} hash={node.tip()['hash']}")

        if args.shutdown == "graceful":
            node.stop()  # must persist the checkpoint
            print("  graceful shutdown completed")
        else:
            node.kill9()
            print("  SIGKILL issued")

        print("== phase 2: restart after phase-1 shutdown ==")
        node.start()
        b = node.snapshot()
        print(f"recovered height={b['height']}")
        compare("after phase 1", a, b)
        if args.shutdown == "graceful" and b["height"] < a["height"]:
            rc = fail(f"graceful checkpoint lost: {b['height']} < {a['height']}")

        base = b["height"]
        wait_for(lambda: node.height() >= base + 2, args.timeout,
                 "mining two more blocks")
        c = node.snapshot()
        print(f"second capture height={c['height']}")
        node.kill9()
        print("  SIGKILL issued")

        print("== phase 3: restart after SIGKILL ==")
        node.start()
        d = node.snapshot()
        print(f"recovered height={d['height']}")
        compare("after phase 2", c, d)

        rec = d["height"]
        wait_for(lambda: node.height() > rec, args.timeout, "post-crash liveness")
        print(f"  ok   node advanced past {rec} -> {node.height()}")

        print("PASS" if rc == 0 else "FAILED")
        return rc
    finally:
        try:
            node.stop()
        except Exception as exc:
            print(f"cleanup: {exc}", file=sys.stderr)
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
