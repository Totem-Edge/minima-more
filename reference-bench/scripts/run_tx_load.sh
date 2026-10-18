#!/bin/sh
# Metric 4 — CPU under transaction load.
#
# Drives each node with real transactions over RPC (send = sign + TxPoW mine +
# mempool/cascade/MMR). The node is paced by its own spendable coin (solo automine),
# so we cap the offered rate with RATE tx/s and measure the *achieved* rate, the
# CPU-seconds the node consumed while serving them, and per-tx latency.
#
#   N=5 (successful sends target)  RATE=1 (max offered tx/s)  NODE=cpp|java|both
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/_env.sh"
. "$HERE/_node.sh"

N=${N:-5}
RATE=${RATE:-1}
CONFIRM_TIMEOUT=${CONFIRM_TIMEOUT:-180}
NODE=${NODE:-both}
OUT=${1:-$_BENCH_DIR/results/$(date +%Y%m%d-%H%M%S)-$(uname -m)}
mkdir -p "$OUT"
echo "Results dir: $OUT  (N=$N sends, rate cap ${RATE}/s)"

load() {
    name=$1; port=$2; shift 2
    dir=/tmp/txload-$name; rm -rf "$dir"; mkdir -p "$dir"
    rpc=$((port + 4)); base="http://127.0.0.1:$rpc"
    "$@" > "$OUT/${name}_txload.log" 2>&1 &
    pid=$!
    wait_ready "$OUT/${name}_txload.log" || true
    for i in $(seq 1 600); do curl -s -m3 -u minima:test "$base/status" >/dev/null 2>&1 && break; sleep 0.2; done
    addr=$(curl -s -m6 -u minima:test "$base/getaddress" | grep -o '"address":"[^"]*"' | head -1 | cut -d'"' -f4)

    # wait for first spendable coin (warmup)
    for i in $(seq 1 $((CONFIRM_TIMEOUT * 5))); do
        s=$(sendable "$rpc"); [ -z "$s" ] && s=0
        [ "$s" -gt 0 ] 2>/dev/null && break
        sleep 0.2
    done

    : > "$OUT/${name}_txload_latency_ms.txt"
    c0=$(cpusec "$pid"); t0=$(now)
    peak=$(rss "$pid"); ok=0; fail=0; last_ts=0
    for i in $(seq 1 "$N"); do
        # wait until spendable again
        s=0; w=0
        while :; do
            s=$(sendable "$rpc"); [ -z "$s" ] && s=0
            [ "$s" -gt 0 ] 2>/dev/null && break
            w=$((w + 1)); [ "$w" -gt "$CONFIRM_TIMEOUT" ] && break
            sleep 1
        done
        # rate cap: minimum interval between sends
        if [ "$RATE" -gt 0 ] 2>/dev/null; then
            minint=$(awk -v r="$RATE" 'BEGIN{printf "%.3f", 1/r}')
            nowt=$(now)
            if [ "$(awk -v a="$nowt" -v b="$last_ts" -v m="$minint" 'BEGIN{print (a-b<m)?1:0}')" = "1" ]; then
                sleep "$(awk -v a="$nowt" -v b="$last_ts" -v m="$minint" 'BEGIN{printf "%.3f", m-(a-b)}')"
            fi
        fi
        ts=$(now)
        resp=$(curl -s -m 120 -u minima:test "$base/send%20address:$addr%20amount:1")
        te=$(now); last_ts=$te
        if echo "$resp" | grep -q '"status":true'; then
            ok=$((ok + 1))
            awk -v a="$te" -v b="$ts" 'BEGIN{printf "%.1f\n", (a-b)*1000}' >> "$OUT/${name}_txload_latency_ms.txt"
        else
            fail=$((fail + 1))
        fi
        r=$(rss "$pid"); [ -n "$r" ] && [ "$r" -gt "$peak" ] 2>/dev/null && peak=$r
    done
    c1=$(cpusec "$pid"); t1=$(now); thr=$(nthreads "$pid")
    idle=$(rss "$pid")

    wall=$(awk -v a="$t1" -v b="$t0" 'BEGIN{printf "%.2f", a-b}')
    cpu_s=$(awk -v a="$c1" -v b="$c0" 'BEGIN{printf "%.3f", a-b}')
    cpu_per_tx=$(awk -v s="$cpu_s" -v o="$ok" 'BEGIN{ if (o>0) printf "%.0f", s/o*1000; else print 0 }')
    tput=$(awk -v o="$ok" -v w="$wall" 'BEGIN{ if (w>0) printf "%.2f", o/(w/60); else print 0 }')
    lat=$(awk '{n++; s+=$1; if(min==""||$1<min)min=$1; if($1>max)max=$1} END{ if(n>0) printf "%.1f %.1f %.1f", s/n, min, max; else printf "0 0 0" }' "$OUT/${name}_txload_latency_ms.txt")
    stop_pid "$pid"

    {
        echo "node	$name	"
        echo "load_window_s	$wall	s"
        echo "tx_ok	$ok	"
        echo "tx_failed	$fail	"
        echo "achieved_tx_per_min	$tput	tx/min"
        echo "cpu_seconds_load	$cpu_s	s"
        echo "cpu_ms_per_tx	$cpu_per_tx	ms/tx"
        echo "send_latency_avg_ms	$(echo "$lat" | cut -d' ' -f1)	ms"
        echo "send_latency_min_ms	$(echo "$lat" | cut -d' ' -f2)	ms"
        echo "send_latency_max_ms	$(echo "$lat" | cut -d' ' -f3)	ms"
        echo "peak_rss_kb	$peak	kB"
        echo "idle_rss_kb	$idle	kB"
        echo "threads	$thr	"
    } | tee "$OUT/${name}_txload_summary.txt"
}

case "$NODE" in
  cpp|both)  load cpp  21301 "$CPP_BIN"  -data /tmp/txload-cpp  -solo -daemon -clean -port 21301 -rpcenable -rpcpassword test ;;
esac
case "$NODE" in
  java|both) load java 21401 "$JDK/java" -cp "$(jcp)" org.minima.Minima -data /tmp/txload-java -solo -daemon -clean -port 21401 -rpcenable -rpcpassword test ;;
esac

g() { grep -E "^$2\b" "$OUT/$1_txload_summary.txt" 2>/dev/null | head -1 | cut -f2; }
if [ -f "$OUT/cpp_txload_summary.txt" ] || [ -f "$OUT/java_txload_summary.txt" ]; then
    {
        echo "# CPU under transaction load (N=$N sends, rate cap ${RATE}/s)"
        echo
        echo "| Metric | C++ (this node) | Java (minima-core) |"
        echo "|---|---|---|"
        echo "| sends ok / failed | $(g cpp tx_ok) / $(g cpp tx_failed) | $(g java tx_ok) / $(g java tx_failed) |"
        echo "| achieved throughput (tx/min) | $(g cpp achieved_tx_per_min) | $(g java achieved_tx_per_min) |"
        echo "| CPU-seconds during load | $(g cpp cpu_seconds_load) | $(g java cpu_seconds_load) |"
        echo "| CPU milliseconds per tx | $(g cpp cpu_ms_per_tx) | $(g java cpu_ms_per_tx) |"
        echo "| send latency avg (ms) | $(g cpp send_latency_avg_ms) | $(g java send_latency_avg_ms) |"
        echo "| peak RSS (kB) | $(g cpp peak_rss_kb) | $(g java peak_rss_kb) |"
        echo
        echo "Each send = sign + TxPoW mine + mempool/cascade/MMR. Achieved rate is"
        echo "bounded by solo automine cadence (spendable coin). Single run."
    } | tee "$OUT/TXLOAD_SUMMARY.md"
fi
echo "DONE -> $OUT"
