#!/bin/sh
# Mainnet IBD harness — bootstrap from a real Minima mainnet node and sync the
# LIVE chain (no -test, no -solo => mainnet). Tracks chain length / weight, RSS,
# threads and data-dir growth, and reports whether IBD is progressing.
#
#   PEERS=megammr.minima.global:9001   NODE=cpp|java
#   MAX_SECONDS=1800   SAMPLE=15   STABLE_SECONDS=90   REUSE=0
#
# NOTE: a full mainnet sync can take hours/days and tens of GB. MAX_SECONDS caps
# the observation window; set REUSE=1 to continue a previous run's data dir.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/_env.sh"
. "$HERE/_node.sh"

PEERS=${PEERS:-megammr.minima.global:9001}
NODE=${NODE:-cpp}
MAX_SECONDS=${MAX_SECONDS:-1800}
SAMPLE=${SAMPLE:-15}
STABLE_SECONDS=${STABLE_SECONDS:-90}
REUSE=${REUSE:-0}
OUT=${1:-$_BENCH_DIR/results/$(date +%Y%m%d-%H%M%S)-$(uname -m)}
mkdir -p "$OUT"
echo "Results dir: $OUT  (peer=$PEERS node=$NODE max=${MAX_SECONDS}s)"

name=$NODE
dir=/tmp/mainnet-$name
if [ "$REUSE" != "1" ]; then rm -rf "$dir"; fi
mkdir -p "$dir"
case "$name" in cpp) port=21701;; java) port=21801;; esac
rpc=$((port + 4))

echo "== starting $name (mainnet, bootstrapping $PEERS) =="
t0=$(now)
pid=$(start_node "$name" "$dir" "$OUT/${name}_mainnet.log" "$port" -p2pnodes "$PEERS")
wait_ready "$OUT/${name}_mainnet.log" || true

: > "$OUT/${name}_mainnet_progress.txt"
peak=0; last_len=-1; stable=0; elapsed=0; connected=no; first_len=0; first_t=""
echo "t=0s (waiting for first status)"
while [ "$elapsed" -lt "$MAX_SECONDS" ]; do
    sleep "$SAMPLE"; elapsed=$((elapsed + SAMPLE))

    st=$(rpc "$rpc" status)
    # tip block number = real chain height / IBD progress
    blk=$(echo "$st" | grep -o '"block":[0-9]*' | head -1 | grep -o '[0-9]*' || true)
    [ -z "$blk" ] && blk=0
    # tree length (blocks retained) and cumulative weight as secondary signals
    len=$(echo "$st" | grep -o '"length":[0-9]*' | head -1 | grep -o '[0-9]*' || true)
    [ -z "$len" ] && len=0
    wt=$(echo "$st" | grep -o '"weight":"[0-9]*"' | head -1 | cut -d'"' -f4 || true)
    [ -z "$wt" ] && wt=0
    r=$(rss "$pid"); [ -z "$r" ] && r=0
    [ "$r" -gt "$peak" ] 2>/dev/null && peak=$r
    dk=$(du -sk "$dir" 2>/dev/null | cut -f1); [ -z "$dk" ] && dk=0

    echo "$elapsed	$blk	$len	$wt	$r	$dk" >> "$OUT/${name}_mainnet_progress.txt"
    echo "t=${elapsed}s  block=$blk  length=$len  rss=${r}kB  disk=${dk}kB"

    if [ "$blk" -gt 0 ] 2>/dev/null; then
        connected=yes
        if [ "$first_len" = "0" ]; then first_len=$blk; first_t=$elapsed; fi
    fi
    if [ "$blk" = "$last_len" ]; then stable=$((stable + SAMPLE)); else stable=0; fi
    last_len=$blk
    if [ "$connected" = yes ] && [ "$stable" -ge "$STABLE_SECONDS" ]; then
        echo "height stable for ${stable}s -> considering caught up"; break
    fi
done

rpc "$rpc" status > "$OUT/${name}_final_status.json" 2>/dev/null || true
thr=$(nthreads "$pid")
stop_pid "$pid"

# scan the log for IBD / greeting evidence
greet=$(grep -ciE "greeting|connected|IBD|block download" "$OUT/${name}_mainnet.log" 2>/dev/null || echo 0)
errs=$(grep -ciE "error|exception|terminate|segfault|core dumped" "$OUT/${name}_mainnet.log" 2>/dev/null || echo 0)

{
    echo "node	$name	"
    echo "peer	$PEERS	"
    echo "ibd_progressing	$connected	"
    echo "first_height	$first_len	"
    echo "first_height_at_s	${first_t:-0}	s"
    echo "final_height	${last_len:-0}	"
    echo "window_seconds	$elapsed	s"
    echo "peak_rss_kb	$peak	kB"
    echo "datadir_kb	$(du -sk "$dir" 2>/dev/null | cut -f1)	kB"
    echo "threads	$thr	"
    echo "log_greeting_lines	$greet	"
    echo "log_error_lines	$errs	"
} | tee "$OUT/${name}_mainnet_summary.txt"
echo "DONE -> $OUT"
