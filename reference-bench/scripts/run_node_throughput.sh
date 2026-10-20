#!/bin/sh
# Online throughput harness: drive a solo node with real transactions over RPC.
# TxPoW work happens when a transaction is sent (signing + TxPoW mining +
# mempool/cascade/MMR), so this exercises the full pipeline the optimisations
# target. Only the first spend is immediately possible; each subsequent send must
# wait for the prior change coin to confirm (solo automine ~40 s), so the harness
# paces on "spendable" and times only the successful send calls.
#
# Metrics: cold-boot -> first spendable, per-tx processing latency (send call),
# successes/failures, blocks, peak RSS, threads.
# Scope: performance/resource only - NOT a byte-parity check.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/_env.sh"

CPP_BIN=${CPP_BIN:-$PROJECT_DIR/build-bench/minima}
N=${N:-3}
CONFIRM_TIMEOUT=${CONFIRM_TIMEOUT:-150}
NODE=${NODE:-cpp}          # cpp | java | both
OUT=${1:-$_BENCH_DIR/results/$(date +%Y%m%d-%H%M%S)-$(uname -m)}
mkdir -p "$OUT"
now() { perl -MTime::HiRes=time -e 'print time'; }
rss() { ps -o rss= -p "$1" 2>/dev/null | tr -d ' '; }
# Thread count: Linux `ps -o nlwp`, macOS `ps -M` (probe each).
nthreads() {
    if ps -o nlwp= -p "$1" >/dev/null 2>&1; then
        ps -o nlwp= -p "$1" 2>/dev/null | tr -d ' '
    else
        ps -M "$1" 2>/dev/null | tail -n +2 | wc -l | tr -d ' '
    fi
}

drive() {
    name=$1; port=$2; shift 2
    datadir=/tmp/nbtp-$name
    log=$OUT/${name}_throughput.log
    rm -rf "$datadir"; mkdir -p "$datadir"
    rpcport=$((port + 4))
    base="http://127.0.0.1:$rpcport"

    t0=$(now)
    "$@" > "$log" 2>&1 &
    pid=$!
    peak=0
    for i in $(seq 1 600); do
        r=$(rss "$pid"); [ -n "$r" ] && [ "$r" -gt "$peak" ] 2>/dev/null && peak=$r
        curl -s -m3 -u minima:test "$base/status" >/dev/null 2>&1 && break
        sleep 0.2
    done
    ADDR=$(curl -s -m6 -u minima:test "$base/getaddress" | grep -o '"address":"[^"]*"' | head -1 | cut -d'"' -f4)

    # wait until a coin is spendable
    spendable() { curl -s -m6 -u minima:test "$base/balance" | grep -o '"sendable":"[0-9]*"' | head -1 | grep -o '[0-9]*'; }
    s=0
    for i in $(seq 1 $((CONFIRM_TIMEOUT*5))); do
        r=$(rss "$pid"); [ -n "$r" ] && [ "$r" -gt "$peak" ] 2>/dev/null && peak=$r
        s=$(spendable); [ -z "$s" ] && s=0
        [ "$s" -gt 0 ] 2>/dev/null && break
        sleep 0.2
    done
    warmup=$(perl -MTime::HiRes=time -e "print time-$t0")
    len0=$(curl -s -m4 -u minima:test "$base/status" | grep -o '"length":[0-9]*' | head -1 | grep -o '[0-9]*'); [ -z "$len0" ] && len0=0

    : > "$OUT/${name}_tx_latency_ms.txt"
    ok=0; fail=0; waited=0
    for i in $(seq 1 "$N"); do
        # pace: wait for spendable again
        s=0; w=0
        while :; do
            s=$(spendable); [ -z "$s" ] && s=0
            [ "$s" -gt 0 ] 2>/dev/null && break
            w=$((w+1)); [ "$w" -gt "$CONFIRM_TIMEOUT" ] && break
            sleep 1
        done
        waited=$((waited + w))
        ts=$(now)
        resp=$(curl -s -m 90 -u minima:test "$base/send%20address:$ADDR%20amount:1")
        te=$(now)
        if echo "$resp" | grep -q '"status":true'; then
            ok=$((ok+1))
            perl -MTime::HiRes=time -e "printf '%.1f\n', ($te-$ts)*1000" >> "$OUT/${name}_tx_latency_ms.txt"
        else
            fail=$((fail+1))
            echo "$resp" | head -c 200 >> "$OUT/${name}_throughput.log"
        fi
        r=$(rss "$pid"); [ -n "$r" ] && [ "$r" -gt "$peak" ] 2>/dev/null && peak=$r
    done
    len1=$(curl -s -m4 -u minima:test "$base/status" | grep -o '"length":[0-9]*' | head -1 | grep -o '[0-9]*'); [ -z "$len1" ] && len1=0
    stats=$(awk '{n++; s+=$1; if(min==""||$1<min)min=$1; if($1>max)max=$1} END{ if(n>0) printf "%.1f %.1f %.1f %d", s/n, min, max, n; else printf "0 0 0 0" }' "$OUT/${name}_tx_latency_ms.txt")
    avg=$(echo "$stats" | cut -d' ' -f1)
    mn=$(echo "$stats" | cut -d' ' -f2)
    mx=$(echo "$stats" | cut -d' ' -f3)
    idle=$(rss "$pid"); thr=$(nthreads "$pid")
    kill "$pid" 2>/dev/null || true; sleep 1; kill -9 "$pid" 2>/dev/null || true

    {
        echo "node	$name	"
        echo "warmup_to_spendable_s	$(perl -e "printf '%.2f',$warmup")	s"
        echo "tx_sent_ok	$ok	"
        echo "tx_failed	$fail	"
        echo "send_latency_avg_ms	$avg	ms"
        echo "send_latency_min_ms	$mn	ms"
        echo "send_latency_max_ms	$mx	ms"
        echo "confirm_wait_total_s	$waited	s"
        echo "blocks_added	$((len1-len0))	"
        echo "peak_rss_kb	$peak	kB"
        echo "idle_rss_kb	$idle	kB"
        echo "threads	$thr	"
    } | tee "$OUT/${name}_throughput_summary.txt"
}

case "$NODE" in
  cpp|both)  drive cpp 20201 "$CPP_BIN" -data /tmp/nbtp-cpp -solo -daemon -clean $(mode_flags) -port 20201 -rpcenable -rpcpassword test ;;
esac
case "$NODE" in
  java|both)
    CP=$(ls "$MINIMA_CORE"/lib/*.jar | tr '\n' ':')"$MC_BUILD"
    drive java 20301 "$JDK/java" -cp "$CP" org.minima.Minima -data /tmp/nbtp-java -solo -daemon -clean -port 20301 -rpcenable -rpcpassword test ;;
esac

if [ -f "$OUT/cpp_throughput_summary.txt" ] && [ -f "$OUT/java_throughput_summary.txt" ]; then
  gcp() { grep -E "^$1\b" "$OUT/cpp_throughput_summary.txt" | head -1 | cut -f2; }
  gja() { grep -E "^$1\b" "$OUT/java_throughput_summary.txt" | head -1 | cut -f2; }
  {
    echo "# Online transaction-throughput comparison (solo, N=$N sends)"
    echo
    echo "Each send = sign + TxPoW mine + mempool/cascade/MMR. Subsequent sends wait"
    echo "for the prior change coin to confirm (solo automine cadence), so the useful"
    echo "figure is the per-tx send latency, not wall-clock tx/s."
    echo
    echo "| Metric | C++ (this node) | Java (minima-core) |"
    echo "|---|---|---|"
    echo "| cold-boot -> spendable (s) | $(gcp warmup_to_spendable_s) | $(gja warmup_to_spendable_s) |"
    echo "| sends ok / failed | $(gcp tx_sent_ok) / $(gcp tx_failed) | $(gja tx_sent_ok) / $(gja tx_failed) |"
    echo "| send latency avg (ms) | $(gcp send_latency_avg_ms) | $(gja send_latency_avg_ms) |"
    echo "| send latency min/max (ms) | $(gcp send_latency_min_ms)/$(gcp send_latency_max_ms) | $(gja send_latency_min_ms)/$(gja send_latency_max_ms) |"
    echo "| blocks added | $(gcp blocks_added) | $(gja blocks_added) |"
    echo "| peak RSS (kB) | $(gcp peak_rss_kb) | $(gja peak_rss_kb) |"
    echo "| threads | $(gcp threads) | $(gja threads) |"
    echo
    echo "Single run; indicative. See *_tx_latency_ms.txt for raw samples."
  } | tee "$OUT/THROUGHPUT_SUMMARY.md"
fi
echo "DONE -> $OUT"
