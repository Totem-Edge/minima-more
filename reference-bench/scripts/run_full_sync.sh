#!/bin/sh
# Metrics 1 + 2 — fully-synced node RAM and initial blockchain sync time.
#
# A miner builds a private chain (-test -genesis) for MINE_SECONDS; then, for each
# implementation, a *fresh* node IBMs that chain over P2P from the same miner to
# the SAME target height. We measure:
#   metric 2  sync time   : fresh node start -> tip == miner height
#   metric 1  synced RAM  : peak RSS during sync + idle RSS after sync
#
# Both nodes run on the same host, so run one implementation at a time (default).
# Automine yields ~1 block / 40-50 s, so MINE_SECONDS controls the chain size.
# Scope: performance/resource only - NOT a byte-parity check.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/_env.sh"
. "$HERE/_node.sh"

MINE_SECONDS=${MINE_SECONDS:-300}
T=${T:-60}                        # idle sampling seconds after sync
MINER_TYPE=${MINER_TYPE:-cpp}     # cpp | java
FOLLOWERS=${FOLLOWERS:-"cpp java"}
OUT=${1:-$_BENCH_DIR/results/$(date +%Y%m%d-%H%M%S)-$(uname -m)}
mkdir -p "$OUT"
echo "Results dir: $OUT  (MINE_SECONDS=$MINE_SECONDS, idle T=${T}s)"

MPORT=20601; MRPC=20605
rm -rf /tmp/fs-miner; mkdir -p /tmp/fs-miner
echo "== miner ($MINER_TYPE) building chain =="
MPID=$(start_node "$MINER_TYPE" /tmp/fs-miner "$OUT/miner.log" "$MPORT" -test -genesis -clean)
wait_ready "$OUT/miner.log" || true
for i in $(seq 1 900); do [ -n "$(statuslen $MRPC)" ] && break; sleep 0.2; done
sleep "$MINE_SECONDS"
MLEN=$(statuslen $MRPC); [ -z "$MLEN" ] && MLEN=0
echo "miner height = $MLEN"

i=0
for type in $FOLLOWERS; do
    i=$((i+1))
    port=$((20700 + i * 100)); rpc=$((port + 4))
    dir=/tmp/fs-$type; rm -rf "$dir"; mkdir -p "$dir"
    echo "== follower $type (port $port) IBD to height $MLEN =="

    t0=$(now)
    FPID=$(start_node "$type" "$dir" "$OUT/$type-follower.log" "$port" -test -p2prootnode 127.0.0.1:$MPORT -clean)
    peak=0; synced=no
    for k in $(seq 1 7200); do
        r=$(rss "$FPID"); [ -n "$r" ] && [ "$r" -gt "$peak" ] 2>/dev/null && peak=$r
        fl=$(statuslen $rpc); [ -z "$fl" ] && fl=0
        if [ "$MLEN" -gt 0 ] && [ "$fl" -ge "$MLEN" ] 2>/dev/null; then synced=yes; break; fi
        sleep 0.25
    done
    t1=$(now)
    sync_s=$(awk -v a="$t1" -v b="$t0" 'BEGIN{printf "%.2f", a-b}')

    min=0; max=0; sum=0; n=0
    for k in $(seq 1 "$T"); do
        r=$(rss "$FPID"); [ -z "$r" ] && break
        { [ "$min" -eq 0 ] || [ "$r" -lt "$min" ]; } && min=$r
        [ "$r" -gt "$max" ] && max=$r
        sum=$((sum + r)); n=$((n + 1)); sleep 1
    done
    avg=0; [ "$n" -gt 0 ] && avg=$((sum / n))
    idle=$(rss "$FPID"); thr=$(nthreads "$FPID")
    fl=$(statuslen $rpc); [ -z "$fl" ] && fl=0
    weight=$(curl -s -m6 -u minima:test "http://127.0.0.1:$rpc/status" | grep -o '"weight":"[0-9]*"' | head -1 | cut -d'"' -f4 || true)
    stop_pid "$FPID"

    {
        echo "node	$type	"
        echo "sync_seconds	$sync_s	s"
        echo "sync_complete	$synced	"
        echo "chain_height	$fl	"
        echo "peak_sync_rss_kb	$peak	kB"
        echo "idle_rss_min_kb	$min	kB"
        echo "idle_rss_avg_kb	$avg	kB"
        echo "idle_rss_max_kb	$max	kB"
        echo "idle_rss_last_kb	$idle	kB"
        echo "threads	$thr	"
    } | tee "$OUT/${type}_sync_summary.txt"
done
stop_pid "$MPID"

g() { grep -E "^$2\b" "$OUT/$1_sync_summary.txt" 2>/dev/null | head -1 | cut -f2; }
{
    echo "# Initial sync + fully-synced RAM (chain height $(g cpp chain_height)/$(g java chain_height))"
    echo
    echo "Miner built the chain for ${MINE_SECONDS}s; each follower is a **fresh** node"
    echo "that IBDs the same chain from the same miner."
    echo
    echo "| Metric | C++ (this node) | Java (minima-core) |"
    echo "|---|---|---|"
    echo "| sync time (s) | $(g cpp sync_seconds) | $(g java sync_seconds) |"
    echo "| sync complete | $(g cpp sync_complete) | $(g java sync_complete) |"
    echo "| chain height | $(g cpp chain_height) | $(g java chain_height) |"
    echo "| peak RSS during sync (kB) | $(g cpp peak_sync_rss_kb) | $(g java peak_sync_rss_kb) |"
    echo "| idle RSS after sync, avg (kB) | $(g cpp idle_rss_avg_kb) | $(g java idle_rss_avg_kb) |"
    echo "| idle RSS after sync, max (kB) | $(g cpp idle_rss_max_kb) | $(g java idle_rss_max_kb) |"
    echo "| threads | $(g cpp threads) | $(g java threads) |"
    echo
    echo "Single run, shared host; automine keeps the chain small (~1 block / 40-50 s)."
} | tee "$OUT/SYNC_SUMMARY.md"
echo "DONE -> $OUT"
