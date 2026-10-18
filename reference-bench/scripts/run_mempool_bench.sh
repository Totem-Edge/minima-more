#!/bin/sh
# Component RAM-scaling harness: RSS vs mempool size (transactions / input
# coins), C++ vs Java. The mempool (RamDB) holds unconfirmed TxPoWs and their
# input coins - the other main in-memory growth structure besides the MMR.
#
# Also measures checkForCoinID ("is this coin already in the mempool?", the
# double-spend guard) lookup time: this node keeps an O(1) coin->TxPoW index,
# while the Java reference scans the whole pool linearly. So mempool RAM and
# lookup latency are a real trade-off to quantify.
#
# C++ self-reports process RSS; the Java harness reports JVM heap-in-use and the
# script samples the JVM's peak RSS externally.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/_env.sh"

N=${N:-100000}
COINS=${COINS:-2}
LOOKUPS=${LOOKUPS:-1000}
OUT=${1:-$_BENCH_DIR/results/$(date +%Y%m%d-%H%M%S)-$(uname -m)}
mkdir -p "$OUT"
echo "Results dir: $OUT (N=$N txns, $COINS coins/txn, $LOOKUPS lookups)"

MEMPOOL_SRC="org/minima/database/txpowdb/ram/ram_d_b.cpp org/minima/database/txpowdb/ram/ram_data.cpp org/minima/objects/tx_po_w.cpp org/minima/objects/tx_body.cpp org/minima/objects/tx_header.cpp org/minima/objects/transaction.cpp org/minima/objects/witness.cpp org/minima/objects/coin.cpp org/minima/objects/coin_proof.cpp org/minima/objects/token.cpp org/minima/objects/script_proof.cpp org/minima/objects/magic.cpp org/minima/objects/state_variable.cpp org/minima/objects/address.cpp org/minima/objects/keys/signature.cpp org/minima/objects/keys/signature_proof.cpp org/minima/objects/keys/winternitz.cpp org/minima/objects/keys/tree_key.cpp org/minima/objects/keys/tree_key_node.cpp org/minima/objects/mmr/m_m_r_proof.cpp org/minima/objects/mmr/m_m_r.cpp org/minima/objects/mmr/m_m_r_data.cpp org/minima/objects/mmr/m_m_r_entry.cpp org/minima/objects/mmr/m_m_r_entry_number.cpp org/minima/objects/base/mini_data.cpp org/minima/objects/base/mini_number.cpp org/minima/objects/base/mini_byte.cpp org/minima/objects/base/mini_string.cpp org/minima/utils/crypto.cpp org/minima/utils/streamable.cpp org/minima/utils/fast_byte_array_stream.cpp org/minima/utils/base_converter.cpp org/minima/system/params/general_params.cpp org/minima/system/params/global_params.cpp org/minima/utils/json/j_s_o_n_aware.cpp org/minima/utils/json/j_s_o_n_array.cpp org/minima/utils/json/j_s_o_n_object.cpp org/minima/utils/json/j_s_o_n_stream_aware.cpp org/minima/utils/json/j_s_o_n_value.cpp org/minima/utils/json/j_s_o_n_writer.cpp org/minima/utils/json/item_list.cpp org/minima/utils/json/parser/container_factory.cpp org/minima/utils/json/parser/content_handler.cpp org/minima/utils/json/parser/j_s_o_n_parser.cpp org/minima/utils/json/parser/parse_exception.cpp org/minima/utils/json/parser/yylex.cpp org/minima/utils/json/parser/yytoken.cpp tests/stubs.cpp"

echo "== C++ =="
( cd "$PROJECT_DIR" && $CXX -std=c++17 -O3 -DNDEBUG -I. $OPENSSL_INCLUDE \
    -o /tmp/bench_mempool "$_BENCH_DIR/cpp/bench_mempool.cpp" $MEMPOOL_SRC \
    $OPENSSL_LIB -lssl -lcrypto -lpthread )
/tmp/bench_mempool "$N" "$COINS" "$LOOKUPS" | tee "$OUT/cpp_mempool.txt"

echo "== Java =="
sh "$HERE/build_minima_core.sh" >/dev/null 2>&1
CP=$(ls "$MINIMA_CORE"/lib/*.jar | tr '\n' ':')"$MC_BUILD"
"$JDK/javac" -nowarn -cp "$CP" -d "$MC_BENCH_OUT" "$_BENCH_DIR/java/BenchMempool.java"
"$JDK/java" -Xmx4g -cp "$CP:$MC_BENCH_OUT" BenchMempool "$N" "$COINS" "$LOOKUPS" > "$OUT/java_mempool.txt" 2>&1 &
jp=$!
peak=0
while kill -0 "$jp" 2>/dev/null; do
    r=$(ps -o rss= -p "$jp" 2>/dev/null | tr -d ' ')
    [ -n "$r" ] && [ "$r" -gt "$peak" ] 2>/dev/null && peak=$r
    sleep 0.1
done
wait "$jp" 2>/dev/null || true
echo "java_peak_rss_kb	$peak	kB" >> "$OUT/java_mempool.txt"
cat "$OUT/java_mempool.txt"

cab() { grep -E "^$1\b" "$OUT/cpp_mempool.txt" | head -1 | cut -f2; }
jab() { grep -E "^$1\b" "$OUT/java_mempool.txt" | head -1 | cut -f2; }
{
  echo "# Mempool RAM-scaling comparison (N=$N txns, $COINS coins/txn)"
  echo
  echo "C++ = this node | Java = minima-core \`$(git -C "$MINIMA_CORE" rev-parse --short HEAD)\` (performance reference only)."
  echo
  echo "| Metric | C++ (this node) | Java (minima-core) |"
  echo "|---|---|---|"
  echo "| mempool txns | $(cab mempool_txns) | $(jab mempool_txns) |"
  echo "| bytes/txpow | $(cab mempool_bytes_per_txpow) (RSS) | $(jab mempool_bytes_per_txpow) (heap) |"
  echo "| bytes/coin | $(cab mempool_bytes_per_coin) (RSS) | $(jab mempool_bytes_per_coin) (heap) |"
  echo "| build (ms) | $(cab mempool_build_ms) | $(jab mempool_build_ms) |"
  echo "| checkForCoinID (us/lookup) | $(cab mempool_lookup_us_per) | $(jab mempool_lookup_us_per) |"
  echo "| process peak RSS (kB) | $(cab rss_after_mempool_kb) | $(jab java_peak_rss_kb) |"
  echo
  echo "Notes: C++ rows are process RSS; Java rows are JVM heap-in-use (plus the peak"
  echo "RSS row which is the whole JVM). Heap != RSS. The C++ node keeps an O(1)"
  echo "coin->TxPoW index (extra RAM) which is why its lookup latency is far lower;"
  echo "Java scans the pool linearly. Single run, indicative only."
} | tee "$OUT/RAM_SUMMARY.md"

echo "DONE -> $OUT"
