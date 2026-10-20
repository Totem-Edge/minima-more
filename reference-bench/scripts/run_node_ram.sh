#!/bin/sh
# Full-node RAM soak: run a node solo and sample its process RSS once per second
# for T seconds, reporting min/max/last and the linear slope (kB/s) as a rough
# leak/steady-state indicator. Runs the C++ node and/or the Java node.
#
# Note: without a scripted workload (blocks/txns) the node is mostly idle, so this
# profile is a steady-state / baseline-leak check rather than a load test.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/_env.sh"

CPP_BIN=${CPP_BIN:-$PROJECT_DIR/build-bench/minima}
T=${T:-30}
NODE=${NODE:-cpp}                 # cpp | java | both
OUT=${1:-$_BENCH_DIR/results/$(date +%Y%m%d-%H%M%S)-$(uname -m)}
mkdir -p "$OUT"
rss() { ps -o rss= -p "$1" 2>/dev/null | tr -d ' '; }

soak() {
    name=$1; dir=/tmp/nbram-$1; shift
    rm -rf "$dir"; mkdir -p "$dir"
    "$@" > "$OUT/${name}_ram.log" 2>&1 &
    pid=$!
    # wait for ready
    for i in $(seq 1 600); do grep -q "Daemon mode started" "$OUT/${name}_ram.log" 2>/dev/null && break; sleep 0.1; done
    : > "$OUT/${name}_rss_series.txt"
    min=0; max=0; last=0; n=0; sum=0
    for i in $(seq 1 "$T"); do
        r=$(rss "$pid"); [ -z "$r" ] && break
        echo "$i	$r" >> "$OUT/${name}_rss_series.txt"
        [ "$min" -eq 0 ] || [ "$r" -lt "$min" ] && min=$r
        [ "$r" -gt "$max" ] && max=$r
        last=$r; n=$((n+1)); sum=$((sum+r))
        sleep 1
    done
    kill "$pid" 2>/dev/null || true; sleep 1; kill -9 "$pid" 2>/dev/null || true
    avg=0
    if [ "$n" -gt 0 ]; then avg=$((sum / n)); fi
    slope=$(awk 'NR==1{first=$2; fi=1} {last=$2} END{ if(fi && NR>1) printf "%.3f", (last-first)/(NR-1); else print "0" }' "$OUT/${name}_rss_series.txt")
    {
        echo "node	$name	"
        echo "samples	$n	"
        echo "rss_min_kb	$min	kB"
        echo "rss_avg_kb	$avg	kB"
        echo "rss_max_kb	$max	kB"
        echo "rss_last_kb	$last	kB"
        echo "rss_slope_kb_per_s	$slope	kB/s"
    } | tee "$OUT/${name}_ram_summary.txt"
}

case "$NODE" in
  cpp|both)
    soak cpp "$CPP_BIN" -data /tmp/nbram-cpp -solo -daemon -clean $(mode_flags) -port 19501 ;;
esac
case "$NODE" in
  java|both)
    CP=$(ls "$MINIMA_CORE"/lib/*.jar | tr '\n' ':')"$MC_BUILD"
    soak java "$JDK/java" -cp "$CP" org.minima.Minima -data /tmp/nbram-java -solo -daemon -clean -port 19502 ;;
esac
echo "DONE -> $OUT"
