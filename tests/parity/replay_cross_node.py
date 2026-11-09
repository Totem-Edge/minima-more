#!/usr/bin/env python3
"""Level B (first increment) — cross-node block acceptance.

One C++ node mines a private test chain. The pinned Java node connects to it and
IBDs the same chain. Every block hash for heights 1..H must match on both sides.

This is the first real Level B leg: the Java node validates and accepts the C++
node's blocks over the real P2P/IBD path, rather than through the kernel.

Usage:
  python3 tests/parity/replay_cross_node.py \
      --cpp build/minima --core /path/to/minima-core [--height 5]
"""
import argparse
import base64
import hashlib
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

PASSWORD = "parity-cross-pass"


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


def state_fields(port):
    """Canonical state summary used to compare two nodes after replay.

    Every field is best-effort: the Java oracle and the C++ nodes expose
    slightly different RPC surfaces, so a missing field is omitted rather than
    guessed. Only fields present on both sides are compared.
    """
    fields = {}
    status = rpc(port, "status")["response"]
    chain = status.get("chain") or {}
    fields["height"] = int(chain.get("block", 0))
    if "coins" in status:
        fields["coins"] = str(status["coins"])
    height = fields["height"]
    if height > 0:
        # Consensus state is the tip header, which is what every node must
        # agree on. status chain/cascade "weight" is deliberately NOT used: it
        # is the tree node's *subtree* total, so it includes orphaned side
        # branches that differ legitimately between a miner and an IBD peer.
        tip = rpc(port, f"txpow block:{height}")["response"]
        fields["tip"] = tip["txpowid"].lower()
        header = tip.get("header") or {}
        for key in ("block", "mmr", "total", "blkdiff", "nonce", "timemilli",
                    "customhash", "chainid"):
            if key in header:
                fields[f"tip_{key}"] = str(header[key])
    try:
        verify = rpc(port, "state action:verify")["response"]
        if "ok" in verify:
            fields["verify_ok"] = bool(verify["ok"])
        if verify.get("issues"):
            fields["verify_issues"] = str(verify["issues"])
    except Exception:
        pass
    return fields


def digest_of(fields):
    canonical = json.dumps(fields, sort_keys=True).encode()
    return hashlib.sha256(canonical).hexdigest()


def compare_state(producer, consumer):
    """Compare every state field both nodes expose. Returns a list of diffs."""
    pfields = state_fields(producer.rpc_port)
    cfields = state_fields(consumer.rpc_port)
    shared = sorted(set(pfields) & set(cfields))
    diffs = [
        (key, pfields[key], cfields[key])
        for key in shared
        if pfields[key] != cfields[key]
    ]
    print(f"  state digest {producer.name}={digest_of(pfields)} "
          f"{consumer.name}={digest_of(cfields)}")
    print(f"  compared fields: {', '.join(shared)}")
    return diffs


def wait_for(fn, timeout, what, interval=0.5):
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


class Proc:
    def __init__(self, name, argv, data, port):
        self.name = name
        self.argv = argv
        self.data = data
        self.port = port
        self.rpc_port = port + 4
        self.log = None
        # Test-only mining pause (honoured only under -test). Needed so a state
        # comparison is not a race between two miners.
        self.pause_file = os.path.join(data, "pause")
        self.env = dict(os.environ, MINIMA_TEST_PAUSE_FILE=self.pause_file)

    def pause(self):
        open(self.pause_file, "w").close()

    def resume(self):
        try:
            os.unlink(self.pause_file)
        except FileNotFoundError:
            pass

    def _pids(self):
        try:
            out = subprocess.check_output(
                ["pgrep", "-f", "--", f"-data {self.data}"], text=True)
            return [int(x) for x in out.split()]
        except subprocess.CalledProcessError:
            return []

    def start(self):
        self.log = open(os.path.join(self.data, "node.log"), "ab")
        subprocess.Popen(self.argv, stdout=self.log, stderr=subprocess.STDOUT,
                         env=self.env)
        wait_for(lambda: rpc(self.rpc_port, "status", timeout=3).get("status"),
                 180, f"{self.name} rpc")
        return self

    def height(self):
        return int(rpc(self.rpc_port, "status")["response"]["chain"]["block"])

    def block_id(self, height):
        return rpc(self.rpc_port, f"txpow block:{height}")["response"]["txpowid"].lower()

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
        if self.log:
            self.log.close()
            self.log = None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cpp", default=os.environ.get("MINIMA_NODE", "build/minima"))
    ap.add_argument("--core", default=os.environ.get("MINIMA_CORE", ""))
    ap.add_argument("--java-classes",
                    default=os.environ.get("MINIMA_CORE_BUILD", "/tmp/minima-core-build"))
    ap.add_argument("--jdk", default=os.environ.get("JDK", ""))
    ap.add_argument("--direction",
                    choices=("cpp2java", "java2cpp", "cpp2cpp"),
                    default="cpp2java")
    ap.add_argument("--height", type=int, default=5)
    ap.add_argument("--spend-every", type=int, default=0,
                    help="mine a spend every N blocks on the producer (0 = never)")
    ap.add_argument("--timeout", type=int, default=240)
    ap.add_argument("--state-settle", type=int, default=30,
                    help="seconds to wait for state fields to converge")
    ap.add_argument("--no-state-digest", dest="state_digest",
                    action="store_false",
                    help="skip the post-replay state comparison")
    ap.add_argument("--keep-work", action="store_true")
    args = ap.parse_args()

    if not (os.path.isfile(args.cpp) and os.access(args.cpp, os.X_OK)):
        print(f"FAIL: C++ node not found: {args.cpp}", file=sys.stderr)
        return 2
    needs_java = args.direction != "cpp2cpp"
    if needs_java:
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
    else:
        jdk = ""
        classpath = ""

    work = tempfile.mkdtemp(prefix="parity-cross-")
    cpp_data = os.path.join(work, "cpp")
    java_data = os.path.join(work, "java")
    os.makedirs(cpp_data, exist_ok=True)
    os.makedirs(java_data, exist_ok=True)
    cpp_port = free_port()
    java_port = free_port()
    cpp2_data = os.path.join(work, "cpp2")
    os.makedirs(cpp2_data, exist_ok=True)
    cpp2_port = free_port()

    def cpp_argv(mode, connect=None):
        argv = [args.cpp, "-daemon", "-data", cpp_data, "-port", str(cpp_port),
                "-rpcenable", "true", "-rpcpassword", PASSWORD]
        if mode == "solo":
            return [args.cpp, "-solo"] + argv[1:]
        argv = [args.cpp, "-test", "-allowallip", "true", "-testchainlength", "512",
                "-ibdlimit", "0"] + argv[1:]
        if connect:
            argv += ["-connect", f"127.0.0.1:{connect}"]
        return argv

    def java_argv(mode, connect=None):
        argv = [os.path.join(jdk, "java"), "-cp", classpath, "org.minima.Minima",
                "-daemon", "-data", java_data, "-port", str(java_port),
                "-rpcenable", "true", "-rpcpassword", PASSWORD]
        if mode == "solo":
            return argv[:4] + ["-solo"] + argv[4:]
        argv = argv[:4] + ["-test"] + argv[4:]
        if connect:
            argv += ["-connect", f"127.0.0.1:{connect}"]
        return argv

    cpp = Proc("cpp", cpp_argv("solo"), cpp_data, cpp_port)
    java = Proc("java", java_argv("solo"), java_data, java_port)
    cpp2 = Proc("cpp2", cpp_argv("solo"), cpp2_data, cpp2_port)
    if args.direction == "cpp2java":
        producer, consumer = cpp, java
    elif args.direction == "java2cpp":
        producer, consumer = java, cpp
    else:
        producer, consumer = cpp, cpp2
    producer_port = {id(cpp): cpp_port, id(java): java_port}[id(producer)]

    def producer_test_argv():
        return cpp_argv("test") if producer is cpp else java_argv("test")

    def consumer_test_argv():
        if consumer is cpp2:
            return [args.cpp, "-test", "-allowallip", "true",
                    "-testchainlength", "512", "-ibdlimit", "0", "-daemon",
                    "-data", cpp2_data, "-port", str(cpp2_port),
                    "-rpcenable", "true", "-rpcpassword", PASSWORD,
                    "-connect", f"127.0.0.1:{producer_port}"]
        if consumer is cpp:
            return cpp_argv("test", connect=producer_port)
        return java_argv("test", connect=producer_port)

    def mine_with_spends(proc, target):
        sends = 0
        next_spend = max(2, proc.height()) + max(1, args.spend_every)
        while proc.height() < target:
            height = proc.height()
            if args.spend_every > 0 and height >= next_spend:
                try:
                    address = rpc(proc.rpc_port, "newaddress")["response"]["address"]
                    rpc(proc.rpc_port,
                        f"send amount:0.1 address:{address} mine:true",
                        timeout=180)
                    sends += 1
                    next_spend = proc.height() + args.spend_every
                    continue
                except Exception as exc:
                    print(f"  spend at height {height} skipped: {exc}", flush=True)
                    next_spend = height + args.spend_every
            time.sleep(0.2)
        return sends

    rc = 0
    try:
        print(f"== {producer.name} genesis (-solo) ==", flush=True)
        producer.start()
        wait_for(lambda: producer.height() >= 1, 120, "producer genesis")
        producer.stop()

        print(f"== {producer.name} mines to height {args.height} ==", flush=True)
        producer.argv = producer_test_argv()
        producer.start()
        sends = mine_with_spends(producer, args.height)
        if args.spend_every > 0 and sends < 1:
            print("FAIL: no spend block was mined", file=sys.stderr)
            return 1
        producer_hashes = {h: producer.block_id(h)
                           for h in range(1, producer.height() + 1)}
        print(f"{producer.name} height={producer.height()} spends={sends} "
              f"tip={producer_hashes[producer.height()]}", flush=True)

        print(f"== {consumer.name} connects and IBDs ==", flush=True)
        consumer.argv = consumer_test_argv()
        consumer.start()
        ibd_start = time.time()
        wait_for(lambda: consumer.height() >= args.height, args.timeout,
                 f"{consumer.name} IBD")
        elapsed = time.time() - ibd_start
        height = consumer.height()
        print(f"  {consumer.name} IBD height={height} elapsed={elapsed:.1f}s "
              f"({height / max(elapsed, 0.001):.1f} blocks/s)", flush=True)

        mismatches = []
        for h in range(1, args.height + 1):
            got = consumer.block_id(h)
            if got != producer_hashes[h]:
                mismatches.append((h, producer_hashes[h], got))
        if mismatches:
            for h, want, got in mismatches[:5]:
                print(f"  FAIL height {h}: {consumer.name}={got} "
                      f"{producer.name}={want}", file=sys.stderr)
            rc = 1
        else:
            print(f"  ok   {consumer.name} accepted blocks 1..{args.height} "
                  f"with identical IDs")

        if args.state_digest:
            # Freeze both miners so the comparison is not a race, and let the
            # consumer finish any block already in flight.
            producer.pause()
            consumer.pause()
            try:
                wait_for(lambda: consumer.height() >= producer.height(), 120,
                         "consumer catch-up before state comparison")
            except RuntimeError as exc:
                print(f"  note: {exc}", file=sys.stderr)
            # Pause is honoured before the next mining round, so wait for any
            # in-flight round to land before sampling state.
            time.sleep(4)
            # Weight converges only once any cascade state has propagated, so
            # give both nodes the budget instead of sampling a transient.
            deadline = time.time() + args.state_settle
            while time.time() < deadline:
                pf = state_fields(producer.rpc_port)
                cf = state_fields(consumer.rpc_port)
                if all(pf.get(k) == cf.get(k) for k in set(pf) & set(cf)):
                    break
                time.sleep(2)
            diffs = compare_state(producer, consumer)
            producer.resume()
            consumer.resume()
            if diffs:
                for key, want, got in diffs[:8]:
                    print(f"  FAIL state {key}: {consumer.name}={got} "
                          f"{producer.name}={want}", file=sys.stderr)
                rc = 1
            else:
                print("  ok   replayed state matches on every shared field")

        print(f"  {consumer.name} height={consumer.height()}", flush=True)
        print("PASS" if rc == 0 else "FAILED")
        return rc
    finally:
        for proc in (java, cpp, cpp2):
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
