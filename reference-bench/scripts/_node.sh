#!/bin/sh
# Shared helpers for the node-lifecycle benchmarks (sync, CPU, disk, load).
# Source AFTER _env.sh:
#   . "$HERE/_env.sh"
#   . "$HERE/_node.sh"
#
# All measurements are per-process. CPU and disk-write counters use /proc on
# Linux (the deployment target); macOS falls back to `ps` for CPU and reports
# disk I/O as unavailable.

CPP_BIN=${CPP_BIN:-$PROJECT_DIR/build-bench/minima}

# --- timing / process metrics ---------------------------------------------

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

# Cumulative CPU seconds (user+system) consumed by a pid.
cpusec() {
    if [ -r "/proc/$1/stat" ]; then
        awk -v hz="$(getconf CLK_TCK 2>/dev/null || echo 100)" \
            '{printf "%.3f", ($14 + $15) / hz}' "/proc/$1/stat"
    else
        ps -o time= -p "$1" 2>/dev/null | awk -F'[:.]' '
            NF==4 { printf "%.3f", ($1*3600)+($2*60)+$3+($4/100); next }
            NF==3 { printf "%.3f", ($1*60)+$2+($3/100); next }
            NF==2 { printf "%.3f", $1+($2/100); next }
            { printf "%.3f", $1 }'
    fi
}

# Bytes actually written to storage by a pid (Linux /proc/<pid>/io: write_bytes).
# Prints nothing (and succeeds) where /proc is unavailable (macOS).
iowrite() {
    [ -r "/proc/$1/io" ] || return 0
    awk '/^write_bytes/{print $2}' "/proc/$1/io" 2>/dev/null
}
# Chars passed to write() (includes page cache); useful cross-check.
iowchar() {
    [ -r "/proc/$1/io" ] || return 0
    awk '/^wchar/{print $2}' "/proc/$1/io" 2>/dev/null
}

# --- RPC -------------------------------------------------------------------

jcp() { echo "$(ls "$MINIMA_CORE"/lib/*.jar | tr '\n' ':')$MC_BUILD"; }
rpc() { curl -s -m8 -u minima:test "http://127.0.0.1:$1/$2"; }
# These always succeed (may print nothing) so callers can use them safely under
# `set -e` while the node is still starting up.
statuslen() { rpc "$1" status | grep -o '"length":[0-9]*' | head -1 | grep -o '[0-9]*' || true; }
sendable() { rpc "$1" balance | grep -o '"sendable":"[0-9]*"' | head -1 | grep -o '[0-9]*' || true; }

# --- node launch -----------------------------------------------------------

start_cpp() {  # datadir logfile port extra...
    _d=$1; _l=$2; _p=$3; shift 3
    "$CPP_BIN" -data "$_d" -daemon -allowallip -port "$_p" \
        -rpcenable -rpcpassword test "$@" > "$_l" 2>&1 &
    echo $!
}

start_java() {  # datadir logfile port extra...
    _d=$1; _l=$2; _p=$3; shift 3
    "$JDK/java" -cp "$(jcp)" org.minima.Minima -data "$_d" -daemon -allowallip \
        -port "$_p" -rpcenable -rpcpassword test "$@" > "$_l" 2>&1 &
    echo $!
}

# start <type> <datadir> <logfile> <port> [extra...]
start_node() { _t=$1; shift; case "$_t" in cpp) start_cpp "$@";; java) start_java "$@";; esac; }

wait_ready() {  # logfile
    for _i in $(seq 1 900); do
        grep -q "Daemon mode started" "$1" 2>/dev/null && return 0
        sleep 0.1
    done
    return 1
}

stop_pid() { kill "$1" 2>/dev/null || true; sleep 1; kill -9 "$1" 2>/dev/null || true; }
