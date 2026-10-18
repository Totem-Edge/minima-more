#!/bin/sh
# Metric 3 — CPU usage at idle.
#
# Boots each implementation solo on a fresh chain, waits for it to settle, then
# over IDLE_SECONDS measures the CPU-seconds consumed by the process (from
# /proc/<pid>/stat on Linux; `ps` elsewhere) and converts to an average CPU
# utilisation for that window. This is the "running 24/7" efficiency figure.
#
#   IDLE_SECONDS=3600 (default)  SETTLE=15  NODE=cpp|java|both
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/_env.sh"
. "$HERE/_node.sh"

IDLE_SECONDS=${IDLE_SECONDS:-3600}
SETTLE=${SETTLE:-15}
NODE=${NODE:-both}
OUT=${1:-$_BENCH_DIR/results/$(date +%Y%m%d-%H%M%S)-$(uname -m)}
mkdir -p "$OUT"
echo "Results dir: $OUT  (idle window ${IDLE_SECONDS}s, settle ${SETTLE}s)"

cpu_soak() {
    name=$1; shift
    dir=/tmp/cpuidle-$name; rm -rf "$dir"; mkdir -p "$dir"
    "$@" > "$OUT/${name}_cpu_idle.log" 2>&1 &
    pid=$!
    wait_ready "$OUT/${name}_cpu_idle.log" || true
    sleep "$SETTLE"

    c0=$(cpusec "$pid"); t0=$(now)
    min=0; max=0; sum=0; n=0; elapsed=0
    while [ "$elapsed" -lt "$IDLE_SECONDS" ]; do
        step=10; [ "$((IDLE_SECONDS - elapsed))" -lt 10 ] && step=$((IDLE_SECONDS - elapsed))
        sleep "$step"; elapsed=$((elapsed + step))
        r=$(rss "$pid"); [ -z "$r" ] && break
        { [ "$min" -eq 0 ] || [ "$r" -lt "$min" ]; } && min=$r
        [ "$r" -gt "$max" ] && max=$r
        sum=$((sum + r)); n=$((n + 1))
    done
    c1=$(cpusec "$pid"); t1=$(now); thr=$(nthreads "$pid")

    wall=$(awk -v a="$t1" -v b="$t0" 'BEGIN{printf "%.2f", a-b}')
    cpu_s=$(awk -v a="$c1" -v b="$c0" 'BEGIN{printf "%.3f", a-b}')
    cpu_pct=$(awk -v s="$cpu_s" -v w="$wall" 'BEGIN{ if (w>0) printf "%.3f", s/w*100; else print "0" }')
    cpu_ms_per_h=$(awk -v s="$cpu_s" -v w="$wall" 'BEGIN{ if (w>0) printf "%.0f", s/w*3600*1000; else print 0 }')
    avg=0; [ "$n" -gt 0 ] && avg=$((sum / n))
    stop_pid "$pid"

    {
        echo "node	$name	"
        echo "window_seconds	$wall	s"
        echo "cpu_seconds	$cpu_s	s"
        echo "cpu_percent_avg	$cpu_pct	%"
        echo "cpu_seconds_per_hour	$(awk -v s="$cpu_s" -v w="$wall" 'BEGIN{ if(w>0) printf "%.2f", s/w*3600; else print 0 }')	s/h"
        echo "rss_min_kb	$min	kB"
        echo "rss_avg_kb	$avg	kB"
        echo "rss_max_kb	$max	kB"
        echo "threads	$thr	"
    } | tee "$OUT/${name}_cpu_idle_summary.txt"
}

case "$NODE" in
  cpp|both)  cpu_soak cpp  "$CPP_BIN" -data /tmp/cpuidle-cpp -solo -daemon -clean -port 21101 ;;
esac
case "$NODE" in
  java|both) cpu_soak java "$JDK/java" -cp "$(jcp)" org.minima.Minima -data /tmp/cpuidle-java -solo -daemon -clean -port 21201 ;;
esac

g() { grep -E "^$2\b" "$OUT/$1_cpu_idle_summary.txt" 2>/dev/null | head -1 | cut -f2; }
if [ -f "$OUT/cpp_cpu_idle_summary.txt" ] || [ -f "$OUT/java_cpu_idle_summary.txt" ]; then
    {
        echo "# CPU at idle (window ${IDLE_SECONDS}s)"
        echo
        echo "| Metric | C++ (this node) | Java (minima-core) |"
        echo "|---|---|---|"
        echo "| avg CPU utilisation (%) | $(g cpp cpu_percent_avg) | $(g java cpu_percent_avg) |"
        echo "| CPU-seconds in window | $(g cpp cpu_seconds) | $(g java cpu_seconds) |"
        echo "| CPU-seconds per hour | $(g cpp cpu_seconds_per_hour) | $(g java cpu_seconds_per_hour) |"
        echo "| RSS avg (kB) | $(g cpp rss_avg_kb) | $(g java rss_avg_kb) |"
        echo "| threads | $(g cpp threads) | $(g java threads) |"
        echo
        echo "CPU% = CPU-seconds / wall-seconds. Single run, shared host, idle (no"
        echo "workload) after startup."
    } | tee "$OUT/CPU_IDLE_SUMMARY.md"
fi
echo "DONE -> $OUT"
