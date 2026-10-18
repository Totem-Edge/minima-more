#!/bin/sh
# Full-node comparison: this C++ node vs the Java reference (minima-core).
#
# Both are launched on a FRESH private chain in solo mode (-solo -daemon -clean),
# which forces the deterministic heavy startup path: base seed generation, initial
# key generation, genesis-block creation (keygen + mining + MMR + DB) and daemon
# bring-up. Metrics are taken from the process (RSS, threads, disk) and from the
# log markers, so they are comparable across the two implementations.
#
# Scope: performance/resource only — NOT a byte-parity check (see README.md).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/_env.sh"

CPP_BIN=${CPP_BIN:-$PROJECT_DIR/build-audit/minima}
T=${T:-30}                 # observations after ready (seconds)
OUT=${1:-$_BENCH_DIR/results/$(date +%Y%m%d-%H%M%S)-$(uname -m)}
mkdir -p "$OUT"
echo "Results dir: $OUT"

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

# run_one <name> <datadir> <logfile> <launch-cmd...>
run_one() {
    name=$1; dir=$2; log=$3; shift 3
    rm -rf "$dir"
    mkdir -p "$dir"

    t0=$(now)
    # shellcheck disable=SC2068
    "$@" > "$log" 2>&1 &
    pid=$!

    peak=0
    t_genesis=
    t_ready=
    # poll until ready or 60s
    while :; do
        r=$(rss "$pid"); [ -n "$r" ] && [ "$r" -gt "$peak" ] 2>/dev/null && peak=$r
        el=$(perl -MTime::HiRes=time -e "print time-$t0")
        awk -v a="$el" 'BEGIN{exit !(a>60)}' && break
        [ -z "$t_genesis" ] && grep -q "Genesis block created" "$log" 2>/dev/null && t_genesis=$(now)
        if grep -q "Daemon mode started" "$log" 2>/dev/null; then t_ready=$(now); break; fi
        sleep 0.1
    done

    gsec=$(awk -v a="$t_genesis" -v b="$t0" 'BEGIN{printf "%.3f", a-b}')
    rsec=$(awk -v a="$t_ready" -v b="$t0" 'BEGIN{printf "%.3f", a-b}')
    idle=$(rss "$pid"); thr=$(nthreads "$pid")

    # observe for T seconds (post-ready), track peak RSS
    for i in $(seq 1 $((T*5))); do
        r=$(rss "$pid"); [ -n "$r" ] && [ "$r" -gt "$peak" ] 2>/dev/null && peak=$r
        sleep 0.2
    done
    disk=$(du -sk "$dir" 2>/dev/null | cut -f1)

    kill "$pid" 2>/dev/null || true
    sleep 1
    kill -9 "$pid" 2>/dev/null || true

    {
        echo "node\t$name\t"
        echo "genesis_seconds\t$gsec\ts"
        echo "ready_seconds\t$rsec\ts"
        echo "peak_rss_kb\t$peak\tkB"
        echo "idle_rss_kb\t$idle\tkB"
        echo "threads\t$thr\t"
        echo "disk_kb\t$disk\tkB"
    } | tee "$OUT/${name}_fullnode.txt"
}

echo "== C++ node (solo, fresh) =="
run_one cpp /tmp/nb-cpp "$OUT/cpp_node.log" \
    "$CPP_BIN" -data /tmp/nb-cpp -solo -daemon -clean -port 19401

echo "== Java node (solo, fresh) =="
CP=$(ls "$MINIMA_CORE"/lib/*.jar | tr '\n' ':')"$MC_BUILD"
run_one java /tmp/nb-java "$OUT/java_node.log" \
    "$JDK/java" -cp "$CP" org.minima.Minima -data /tmp/nb-java -solo -daemon -clean -port 19402

# ---- comparison ----
get() { grep -E "^$2\b" "$OUT/$1_fullnode.txt" | head -1 | cut -f2; }
{
  echo "# Full-node comparison (solo, fresh private chain)"
  echo
  echo "C++ run: this node | Java run: minima-core \`$(git -C "$MINIMA_CORE" rev-parse --short HEAD)\` (performance reference only)."
  echo
  echo "| Metric | C++ (this node) | Java (minima-core) |"
  echo "|---|---|---|"
  echo "| genesis creation (s) | $(get cpp genesis_seconds) | $(get java genesis_seconds) |"
  echo "| daemon ready (s) | $(get cpp ready_seconds) | $(get java ready_seconds) |"
  echo "| peak RSS (kB) | $(get cpp peak_rss_kb) | $(get java peak_rss_kb) |"
  echo "| idle RSS (kB) | $(get cpp idle_rss_kb) | $(get java idle_rss_kb) |"
  echo "| threads | $(get cpp threads) | $(get java threads) |"
  echo "| data dir (kB) | $(get cpp disk_kb) | $(get java disk_kb) |"
  echo
  echo "Logs: cpp_node.log, java_node.log"
} | tee "$OUT/FULLNODE_SUMMARY.md"

echo "DONE -> $OUT"
