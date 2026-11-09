#!/usr/bin/env python3
"""Level I — short operational soak.

Runs a solo node for a configurable duration while sampling height, RSS and data
directory size, then performs a graceful restart and checks the checkpoint.

This is a smoke soak (minutes), not a 24/7 soak. It fails on:
  * no chain progress during the run;
  * RSS growth above --max-rss-growth-mb;
  * a restart that loses the checkpoint height;
  * process death.

Usage:
  python3 tests/parity/soak.py --node build/minima --duration 60
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

PASSWORD = "parity-soak-pass"


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


def dir_kb(path):
    total = 0
    for root, _, files in os.walk(path):
        for name in files:
            try:
                total += os.path.getsize(os.path.join(root, name))
            except OSError:
                pass
    return total // 1024


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
            try:
                if rpc(self.rpc_port, "status", timeout=2).get("status"):
                    return
            except Exception:
                pass
            time.sleep(0.25)
        raise RuntimeError("node did not become ready")

    def height(self):
        return int(rpc(self.rpc_port, "status")["response"]["chain"]["block"])

    def rss_kb(self):
        pids = self._pids()
        if not pids:
            return 0
        try:
            out = subprocess.check_output(
                ["ps", "-o", "rss=", "-p", str(pids[0])], text=True)
            return int(out.strip() or 0)
        except Exception:
            return 0

    def stop(self):
        try:
            rpc(self.rpc_port, "quit", timeout=3)
        except Exception:
            pass
        end = time.time() + 120
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
    ap.add_argument("--duration", type=int, default=60)
    ap.add_argument("--interval", type=int, default=5)
    ap.add_argument("--max-rss-growth-mb", type=int, default=256)
    ap.add_argument("--keep-work", action="store_true")
    args = ap.parse_args()

    if not (os.path.isfile(args.node) and os.access(args.node, os.X_OK)):
        print(f"FAIL: node binary not found: {args.node}", file=sys.stderr)
        return 1

    work = tempfile.mkdtemp(prefix="parity-soak-")
    node = Node(args.node, work, free_port())
    failures = []
    try:
        node.start()
        start = time.time()
        samples = []
        while time.time() - start < args.duration:
            time.sleep(args.interval)
            try:
                samples.append((int(time.time() - start), node.height(),
                                node.rss_kb(), dir_kb(work)))
            except Exception as exc:
                failures.append(f"sample failed: {exc}")
                break

        if len(samples) < 2:
            failures.append("not enough samples")
        else:
            heights = [s[1] for s in samples]
            rss = [s[2] for s in samples]
            growth_mb = (max(rss) - min(rss)) / 1024.0
            print(f"samples: {len(samples)}")
            print(f"height: {heights[0]} -> {heights[-1]}")
            print(f"rss_kb: {min(rss)} -> {max(rss)} (growth {growth_mb:.1f} MB)")
            print(f"data_kb: {samples[0][3]} -> {samples[-1][3]}")
            if heights[-1] <= heights[0]:
                failures.append("no chain progress during soak")
            if growth_mb > args.max_rss_growth_mb:
                failures.append(f"RSS growth {growth_mb:.1f} MB exceeds "
                                f"{args.max_rss_growth_mb} MB")
            last_height = heights[-1]

        checkpoint = node.height()
        node.stop()
        node.start()
        recovered = node.height()
        print(f"checkpoint: {checkpoint} -> recovered {recovered}")
        if recovered < checkpoint:
            failures.append(f"graceful restart lost height: {recovered} < {checkpoint}")
        node.stop()
    except Exception as exc:
        failures.append(str(exc))
    finally:
        try:
            node.stop()
        except Exception:
            pass
        if args.keep_work:
            print(f"work dir kept: {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)

    if failures:
        for f in failures:
            print(f"FAIL: {f}", file=sys.stderr)
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
