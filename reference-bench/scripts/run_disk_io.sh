#!/bin/sh
# Metric 5 — disk writes per hour (and per day), including SQLite WAL activity.
#
# Samples the process's actual storage writes (Linux /proc/<pid>/io write_bytes)
# over DISK_SECONDS while the node runs, and watches the data directory + WAL for
# growth and checkpoint activity. This is the flash-longevity figure: how many
# bytes the node dirties per hour on a device with no user workload.
#
#   DISK_SECONDS=3600 (default)  SETTLE=15  SAMPLE=30  NODE=cpp|java|both
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/_env.sh"
. "$HERE/_node.sh"

DISK_SECONDS=${DISK_SECONDS:-3600}
SETTLE=${SETTLE:-15}
SAMPLE=${SAMPLE:-30}
NODE=${NODE:-both}
OUT=${1:-$_BENCH_DIR/results/$(date +%Y%m%d-%H%M%S)-$(uname -m)}
mkdir -p "$OUT"
echo "Results dir: $OUT  (disk window ${DISK_SECONDS}s, sample ${SAMPLE}s)"

du_kb() { du -sk "$1" 2>/dev/null | cut -f1; }
wal_kb() { find "$1" -name '*-wal' -exec du -k {} + 2>/dev/null | awk '{s+=$1} END{print s+0}'; }

disk_soak() {
    name=$1; dir=$2; shift 2
    rm -rf "$dir"; mkdir -p "$dir"
    "$@" > "$OUT/${name}_disk_io.log" 2>&1 &
    pid=$!
    wait_ready "$OUT/${name}_disk_io.log" || true
    sleep "$SETTLE"

    io0=$(iowrite "$pid"); [ -z "$io0" ] && io0=0
    wchar0=$(iowchar "$pid"); [ -z "$wchar0" ] && wchar0=0
    du0=$(du_kb "$dir"); [ -z "$du0" ] && du0=0
    t0=$(now)

    : > "$OUT/${name}_disk_series.txt"
    elapsed=0; wal_max=0; ckpt=0; prev_wal=0
    while [ "$elapsed" -lt "$DISK_SECONDS" ]; do
        step=$SAMPLE; [ "$((DISK_SECONDS - elapsed))" -lt "$SAMPLE" ] && step=$((DISK_SECONDS - elapsed))
        sleep "$step"; elapsed=$((elapsed + step))
        io=$(iowrite "$pid"); [ -z "$io" ] && io=$io0
        du=$(du_kb "$dir"); [ -z "$du" ] && du=0
        w=$(wal_kb "$dir")
        if [ "$w" -lt "$prev_wal" ] 2>/dev/null; then ckpt=$((ckpt + 1)); fi
        [ "$w" -gt "$wal_max" ] 2>/dev/null && wal_max=$w
        prev_wal=$w
        echo "$elapsed	$io	$du	$w" >> "$OUT/${name}_disk_series.txt"
    done
    io1=$(iowrite "$pid"); [ -z "$io1" ] && io1=$io0
    wchar1=$(iowchar "$pid"); [ -z "$wchar1" ] && wchar1=$wchar0
    du1=$(du_kb "$dir"); [ -z "$du1" ] && du1=0
    t1=$(now)
    stop_pid "$pid"

    wall=$(awk -v a="$t1" -v b="$t0" 'BEGIN{printf "%.2f", a-b}')
    wbytes=$((io1 - io0)); [ "$wbytes" -lt 0 ] && wbytes=0
    wchars=$((wchar1 - wchar0)); [ "$wchars" -lt 0 ] && wchars=0
    dugrow_kb=$((du1 - du0)); [ "$dugrow_kb" -lt 0 ] && dugrow_kb=0
    ph=$(awk -v b="$wbytes" -v w="$wall" 'BEGIN{ if(w>0) printf "%.0f", b/w*3600; else print 0 }')
    pd=$(awk -v p="$ph" 'BEGIN{printf "%.0f", p*24}')

    {
        echo "node	$name	"
        echo "window_seconds	$wall	s"
        echo "storage_write_bytes	$wbytes	B"
        echo "storage_write_bytes_per_hour	$ph	B/h"
        echo "storage_write_bytes_per_day	$pd	B/day"
        echo "write_chars	$wchars	B"
        echo "datadir_growth_kb	$dugrow_kb	kB"
        echo "wal_max_kb	$wal_max	kB"
        echo "wal_checkpoints	$ckpt	"
    } | tee "$OUT/${name}_disk_io_summary.txt"
}

case "$NODE" in
  cpp|both)  disk_soak cpp  /tmp/diskio-cpp  "$CPP_BIN" -data /tmp/diskio-cpp -solo -daemon -clean $(mode_flags) -port 21501 ;;
esac
case "$NODE" in
  java|both) disk_soak java /tmp/diskio-java "$JDK/java" -cp "$(jcp)" org.minima.Minima -data /tmp/diskio-java -solo -daemon -clean -port 21601 ;;
esac

g() { grep -E "^$2\b" "$OUT/$1_disk_io_summary.txt" 2>/dev/null | head -1 | cut -f2; }
if [ -f "$OUT/cpp_disk_io_summary.txt" ] || [ -f "$OUT/java_disk_io_summary.txt" ]; then
    {
        echo "# Disk writes (window ${DISK_SECONDS}s)"
        echo
        echo "| Metric | C++ (this node) | Java (minima-core) |"
        echo "|---|---|---|"
        echo "| storage writes (B/h) | $(g cpp storage_write_bytes_per_hour) | $(g java storage_write_bytes_per_hour) |"
        echo "| storage writes (B/day) | $(g cpp storage_write_bytes_per_day) | $(g java storage_write_bytes_per_day) |"
        echo "| datadir growth (kB) | $(g cpp datadir_growth_kb) | $(g java datadir_growth_kb) |"
        echo "| WAL max (kB) | $(g cpp wal_max_kb) | $(g java wal_max_kb) |"
        echo "| WAL checkpoints | $(g cpp wal_checkpoints) | $(g java wal_checkpoints) |"
        echo
        echo "storage writes = /proc/<pid>/io write_bytes (bytes dirtied to storage)."
        echo "macOS has no equivalent and reports 0. Single run, idle node."
    } | tee "$OUT/DISK_SUMMARY.md"
fi
echo "DONE -> $OUT"
