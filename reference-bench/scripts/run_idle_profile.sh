#!/bin/sh
# Idle profiling for the C++ node (RFC: measured optimisation loop).
#
# Runs a solo node and samples RSS (total / anonymous / file-backed), threads,
# context switches; and, after a settle window, collects a perf profile, syscall
# summary (strace -c) and per-process wake-ups (pidstat -w). Safe to run on a
# box without perf/strace/pidstat: missing tools are skipped.
#
#   DURATION=1800 SAMPLE=30 sh run_idle_profile.sh [OUT dir]
#
# Env: BIN (default ./build-mcp/minima), PORT, SETTLE (default 120).
set -eu
DURATION=${DURATION:-1800}
SAMPLE=${SAMPLE:-30}
PORT=${PORT:-21901}
SETTLE=${SETTLE:-120}
BIN=${BIN:-$PWD/build-mcp/minima}
OUT=${1:-/tmp/idle-profile}

rm -rf "$OUT"; mkdir -p "$OUT/data"
"$BIN" -data "$OUT/data" -solo -daemon -clean -port "$PORT" \
    -rpcenable -rpcpassword test > "$OUT/node.log" 2>&1 &
PID=$!
for _i in $(seq 1 300); do
    grep -q "Daemon mode started" "$OUT/node.log" 2>/dev/null && break
    sleep 0.2
done
echo "pid=$PID settle=${SETTLE}s duration=${DURATION}s sample=${SAMPLE}s"

# Background profilers (after the settle window so startup noise is excluded).
(
    sleep "$SETTLE"
    if command -v perf >/dev/null 2>&1; then
        perf record -F 49 -g -p "$PID" -o "$OUT/perf.data" -- sleep 60 >"$OUT/perf.log" 2>&1 || true
        perf report --stdio -i "$OUT/perf.data" --no-children --percent-limit 0.5 \
            >"$OUT/perf-report.txt" 2>&1 || true
    fi
) &
(
    sleep "$SETTLE"
    if command -v strace >/dev/null 2>&1; then
        strace -c -f -p "$PID" -w -o "$OUT/strace.txt" & _sp=$!
        sleep 60
        kill -INT "$_sp" 2>/dev/null || true
        wait "$_sp" 2>/dev/null || true
    fi
) &
(
    sleep "$SETTLE"
    if command -v pidstat >/dev/null 2>&1; then
        pidstat -w -p "$PID" 5 12 > "$OUT/pidstat.txt" 2>&1 || true
    fi
) &

: > "$OUT/series.txt"
_elapsed=0
while [ "$_elapsed" -lt "$DURATION" ]; do
    _ts=$(date +%s)
    _rss=$(awk '/VmRSS/{print $2}' "/proc/$PID/status" 2>/dev/null || echo 0)
    _anon=$(awk '/^Anonymous:/{print $2}' "/proc/$PID/smaps_rollup" 2>/dev/null || echo 0)
    _rssr=$(awk '/^Rss:/{print $2}' "/proc/$PID/smaps_rollup" 2>/dev/null || echo 0)
    _pss=$(awk '/^Pss:/{print $2}' "/proc/$PID/smaps_rollup" 2>/dev/null || echo 0)
    _thr=$(awk '/^Threads:/{print $2}' "/proc/$PID/status" 2>/dev/null || echo 0)
    _vc=$(awk '/^voluntary_ctxt_switches:/{print $2}' "/proc/$PID/status" 2>/dev/null || echo 0)
    _nc=$(awk '/^nonvoluntary_ctxt_switches:/{print $2}' "/proc/$PID/status" 2>/dev/null || echo 0)
    echo "$_ts rss_kb=$_rss anon_kb=$_anon rss_rollup_kb=$_rssr pss_kb=$_pss threads=$_thr vol_cs=$_vc nonvol_cs=$_nc" >> "$OUT/series.txt"
    sleep "$SAMPLE"
    _elapsed=$((_elapsed + SAMPLE))
done

cp "/proc/$PID/smaps_rollup" "$OUT/smaps_rollup.txt" 2>/dev/null || true
cat "/proc/$PID/status" > "$OUT/status.txt" 2>/dev/null || true
kill "$PID" 2>/dev/null || true
sleep 1
kill -9 "$PID" 2>/dev/null || true
wait 2>/dev/null || true
echo "DONE -> $OUT"
