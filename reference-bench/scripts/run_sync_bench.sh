#!/bin/sh
# Two-node P2P sync/relay harness. A miner builds a private chain (-test -genesis)
# and a fresh follower IBDs it over P2P, then we measure live block relay.
# Miner and follower can be C++ (this node) or Java (minima-core), so both
# same-impl and cross-impl sync can be measured.
#   - IBD latency: follower connect -> tip == miner tip
#   - IBD size/blocks (parsed from the follower's "Initial Block Download" log)
#   - live relay latency: miner mines a block -> follower sees it
#   - RSS
#
# Localhost P2P is blocked by default; both nodes run with -allowallip.
# NB: solo/test automine yields ~1 block / 40-50 s, so the chain is small unless
# MINE_SECONDS is large; this measures the sync/relay mechanism and latency.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/_env.sh"

CPP_BIN=${CPP_BIN:-$PROJECT_DIR/build-bench/minima}
MINE_SECONDS=${MINE_SECONDS:-150}
MINER_TYPE=${MINER_TYPE:-cpp}        # cpp | java
FOLLOWER_TYPE=${FOLLOWER_TYPE:-cpp}  # cpp | java
OUT=${1:-$_BENCH_DIR/results/$(date +%Y%m%d-%H%M%S)-$(uname -m)}
mkdir -p "$OUT"
now() { perl -MTime::HiRes=time -e 'print time'; }
rss() { ps -o rss= -p "$1" 2>/dev/null | tr -d ' '; }
lenof() { curl -s -m4 -u minima:test "http://127.0.0.1:$1/status" | grep -o '"length":[0-9]*' | head -1 | grep -o '[0-9]*' || true; }
jcp() { echo "$(ls "$MINIMA_CORE"/lib/*.jar | tr '\n' ':')$MC_BUILD"; }

start_node() { # type datadir log port extra...
    type=$1; dir=$2; log=$3; port=$4; shift 4
    case "$type" in
        cpp) "$CPP_BIN" -data "$dir" -daemon -allowallip -port "$port" -rpcenable -rpcpassword test "$@" > "$log" 2>&1 & ;;
        java) "$JDK/java" -cp "$(jcp)" org.minima.Minima -data "$dir" -daemon -allowallip -port "$port" -rpcenable -rpcpassword test "$@" > "$log" 2>&1 & ;;
    esac
    echo $!
}

MPORT=20601; FPORT=20701; MRPC=20605; FRPC=20705
rm -rf /tmp/sync-miner /tmp/sync-follower; mkdir -p /tmp/sync-miner /tmp/sync-follower

echo "== miner ($MINER_TYPE) =="
MPID=$(start_node "$MINER_TYPE" /tmp/sync-miner "$OUT/miner.log" "$MPORT" -test -genesis -clean)
for i in $(seq 1 900); do l=$(lenof $MRPC); if [ -n "$l" ]; then break; fi; sleep 0.2; done
sleep "$MINE_SECONDS"
MLEN=$(lenof $MRPC); mpeak=$(rss "$MPID")
echo "miner length=$MLEN rss=$mpeak kB"

echo "== follower ($FOLLOWER_TYPE) IBD =="
t0=$(now)
FPID=$(start_node "$FOLLOWER_TYPE" /tmp/sync-follower "$OUT/follower.log" "$FPORT" -test -p2prootnode 127.0.0.1:$MPORT -clean)
for i in $(seq 1 900); do l=$(lenof $FRPC); if [ -n "$l" ]; then break; fi; sleep 0.2; done
for i in $(seq 1 600); do
    FLEN=$(lenof $FRPC); [ -z "$FLEN" ] && FLEN=0
    if [ "$FLEN" -ge "$MLEN" ] 2>/dev/null; then break; fi
    sleep 0.5
done
ibd_s=$(perl -e "printf '%.2f', $(now)-$t0")
FLEN=$(lenof $FRPC); f_rss_after=$(rss "$FPID")
ibd_blocks=$(grep -o "blocks:[0-9]*" "$OUT/follower.log" | head -1 | grep -o '[0-9]*' || echo 0)
ibd_size=$(grep -o "size:[0-9.]* KB" "$OUT/follower.log" | head -1 | grep -o '[0-9.]*' || echo 0)

echo "== live relay =="
for i in $(seq 1 240); do ml=$(lenof $MRPC); [ -z "$ml" ] && ml=0; if [ "$ml" -gt "$MLEN" ] 2>/dev/null; then break; fi; sleep 0.5; done
tnew=$(now); newlen=$(lenof $MRPC); relay="-"
for i in $(seq 1 600); do
    fl=$(lenof $FRPC); [ -z "$fl" ] && fl=0
    if [ "$fl" -ge "$newlen" ] 2>/dev/null; then relay=$(perl -e "printf '%.0f', ($(now)-$tnew)*1000"); break; fi
    sleep 0.1
done
fpeak=$(rss "$FPID")
kill "$MPID" "$FPID" 2>/dev/null || true; sleep 1; kill -9 "$MPID" "$FPID" 2>/dev/null || true

{
    echo "node	${MINER_TYPE}-${FOLLOWER_TYPE}	"
    echo "mine_seconds	$MINE_SECONDS	s"
    echo "miner_blocks	$MLEN	"
    echo "ibd_seconds	$ibd_s	s"
    echo "ibd_blocks	$ibd_blocks	"
    echo "ibd_size_kb	$ibd_size	KB"
    echo "relay_latency_ms	$relay	ms"
    echo "miner_rss_kb	$mpeak	kB"
    echo "follower_rss_after_sync_kb	$f_rss_after	kB"
    echo "follower_peak_rss_kb	$fpeak	kB"
} | tee "$OUT/sync_summary.txt"
echo "DONE -> $OUT"
