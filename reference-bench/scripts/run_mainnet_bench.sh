#!/bin/sh
# Live-mainnet benchmark: a fresh C++ node vs a fresh Java (minima-core) node,
# each syncing the REAL mainnet chain from the same peers, then settling.
#
# Metrics: time-to-tip, peak RSS during sync, idle RSS after settle, CPU-seconds
# over the settle window, threads, data-dir size.
#
# A unique advertised port is used per run: live peers rate-limit IBDs to the same
# address for ~30 min (see RESULTS_LOG / MAINNET_TESTS.md), so reusing a port
# makes fresh-sync attempts fail.  Override with PORT_CPP / PORT_JAVA.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/_env.sh"
. "$HERE/_node.sh"

PEERS=${PEERS:-"141.148.226.161:9001,65.109.31.226:9001,93.93.105.231:9001"}
NODE=${NODE:-both}          # cpp | java | both
SETTLE=${SETTLE:-60}        # idle sampling window after sync (s)
MAX_WAIT=${MAX_WAIT:-180}   # max wait for first tip (s)
OUT=${1:-$_BENCH_DIR/results/$(date +%Y%m%d-%H%M%S)-$(uname -m)}
mkdir -p "$OUT"
echo "Results dir: $OUT  (peers=$PEERS settle=${SETTLE}s)"

BASE=$((24000 + ($$ % 3000)))

run_one() {
    name=$1; port=$2
    dir=/tmp/mainnet-$name; rm -rf "$dir"; mkdir -p "$dir"
    rpc=$((port + 4))
    t0=$(now)
    pid=$(start_node "$name" "$dir" "$OUT/${name}_mainnet.log" "$port" -p2pnodes "$PEERS")
    wait_ready "$OUT/${name}_mainnet.log" || true

    # p2p_poll <rpcport> -> NP = connections, NK = known peers
    p2p_poll() {
        _n=$(rpc "$1" network 2>/dev/null || true)
        NP=$(echo "$_n" | grep -o '"isconnected":true' | wc -l | tr -d ' '); [ -z "$NP" ] && NP=0
        NK=$(echo "$_n" | grep -o '"numKnownPeers":[0-9]*' | head -1 | grep -o '[0-9]*' || true); [ -z "$NK" ] && NK=0
        return 0
    }
    peak=0; tipms=""; firstpeer=""; n5=""
    for i in $(seq 1 $((MAX_WAIT * 2))); do
        r=$(rss "$pid"); [ -n "$r" ] && [ "$r" -gt "$peak" ] 2>/dev/null && peak=$r
        if [ -z "$tipms" ]; then
            b=$(rpc "$rpc" block | grep -o '"block":"[0-9]*"' | head -1 | grep -o '[0-9]*' || true)
            if [ -n "$b" ] && [ "$b" -gt 1 ] 2>/dev/null; then tipms=$(now); fi
        fi
        p2p_poll "$rpc"
        [ -z "$firstpeer" ] && [ "$NP" -ge 1 ] 2>/dev/null && firstpeer=$(now)
        [ -z "$n5" ] && [ "$NP" -ge 5 ] 2>/dev/null && n5=$(now)
        [ -n "$tipms" ] && break
        sleep 0.5
    done
    synced_s=$(awk -v a="${tipms:-$t0}" -v b="$t0" 'BEGIN{printf "%.2f", a-b}')
    tip=$(rpc "$rpc" block | grep -o '"block":"[0-9]*"' | head -1 | grep -o '[0-9]*' || true)

    c0=$(cpusec "$pid")
    min=0; sum=0; n=0
    for k in $(seq 1 "$SETTLE"); do
        r=$(rss "$pid"); [ -z "$r" ] && break
        { [ "$min" -eq 0 ] || [ "$r" -lt "$min" ]; } && min=$r
        sum=$((sum + r)); n=$((n + 1))
        p2p_poll "$rpc"
        [ -z "$firstpeer" ] && [ "$NP" -ge 1 ] 2>/dev/null && firstpeer=$(now)
        [ -z "$n5" ] && [ "$NP" -ge 5 ] 2>/dev/null && n5=$(now)
        sleep 1
    done
    c1=$(cpusec "$pid"); idle=$(rss "$pid"); thr=$(nthreads "$pid")
    p2p_poll "$rpc"; peers_reached=$NP; known_peers=$NK
    p2p_first_s=$(awk -v a="${firstpeer:-$t0}" -v b="$t0" 'BEGIN{printf "%.2f", a-b}')
    p2p_n5_s=$(awk -v a="${n5:-$t0}" -v b="$t0" 'BEGIN{printf "%.2f", a-b}')
    [ -z "$firstpeer" ] && p2p_first_s="-"
    [ -z "$n5" ] && p2p_n5_s="-"
    dk=$(du -sk "$dir" 2>/dev/null | cut -f1)
    cpu_settle=$(awk -v a="$c1" -v b="$c0" 'BEGIN{printf "%.3f", a-b}')
    avg=0; [ "$n" -gt 0 ] && avg=$((sum / n))
    stop_pid "$pid"

    {
        echo "node	$name	"
        echo "port	$port	"
        echo "synced_seconds	$synced_s	s"
        echo "tip_block	${tip:-0}	"
        echo "peak_sync_rss_kb	$peak	kB"
        echo "idle_rss_avg_kb	$avg	kB"
        echo "idle_rss_last_kb	$idle	kB"
        echo "cpu_seconds_settle	$cpu_settle	s"
        echo "threads	$thr	"
        echo "datadir_kb	$dk	kB"
        echo "p2p_peers_reached	$peers_reached	"
        echo "p2p_known_peers	$known_peers	"
        echo "p2p_time_first_peer_s	$p2p_first_s	s"
        echo "p2p_time_to_5_peers_s	$p2p_n5_s	s"
    } | tee "$OUT/${name}_mainnet_bench.txt"
}

case "$NODE" in cpp|both)  run_one cpp  "${PORT_CPP:-$((BASE + 1))}" ;; esac
case "$NODE" in java|both) run_one java "${PORT_JAVA:-$((BASE + 101))}" ;; esac

g() { grep -E "^$2\b" "$OUT/$1_mainnet_bench.txt" 2>/dev/null | head -1 | cut -f2; }
if [ -f "$OUT/cpp_mainnet_bench.txt" ] || [ -f "$OUT/java_mainnet_bench.txt" ]; then
    {
        echo "# Live-mainnet node benchmark (fresh sync from real peers)"
        echo
        echo "Peers: $PEERS  ·  settle window: ${SETTLE}s  ·  single run, shared host."
        echo
        echo "| Metric | C++ (this node) | Java (minima-core) |"
        echo "|---|---|---|"
        echo "| time to tip (s) | $(g cpp synced_seconds) | $(g java synced_seconds) |"
        echo "| tip block | $(g cpp tip_block) | $(g java tip_block) |"
        echo "| peak RSS during sync (kB) | $(g cpp peak_sync_rss_kb) | $(g java peak_sync_rss_kb) |"
        echo "| idle RSS avg (kB) | $(g cpp idle_rss_avg_kb) | $(g java idle_rss_avg_kb) |"
        echo "| CPU-seconds over ${SETTLE}s settle | $(g cpp cpu_seconds_settle) | $(g java cpu_seconds_settle) |"
        echo "| threads | $(g cpp threads) | $(g java threads) |"
        echo "| data dir (kB) | $(g cpp datadir_kb) | $(g java datadir_kb) |"
        echo "| P2P peers reached | $(g cpp p2p_peers_reached) | $(g java p2p_peers_reached) |"
        echo "| P2P known peers | $(g cpp p2p_known_peers) | $(g java p2p_known_peers) |"
        echo "| P2P time to first peer (s) | $(g cpp p2p_time_first_peer_s) | $(g java p2p_time_first_peer_s) |"
        echo "| P2P time to 5 peers (s) | $(g cpp p2p_time_to_5_peers_s) | $(g java p2p_time_to_5_peers_s) |"
    } | tee "$OUT/MAINNET_BENCH_SUMMARY.md"
fi
echo "DONE -> $OUT"
