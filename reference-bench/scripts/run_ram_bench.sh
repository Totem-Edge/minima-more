#!/bin/sh
# Component RAM-scaling harness: RSS vs number of MMR entries, C++ vs Java.
# MMR is the main persistent-growth structure (grows with the chain). The C++
# harness self-reports process RSS; the Java harness reports JVM heap-in-use and
# the script samples the JVM's peak RSS externally.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/_env.sh"

N=${N:-100000}
OUT=${1:-$_BENCH_DIR/results/$(date +%Y%m%d-%H%M%S)-$(uname -m)}
mkdir -p "$OUT"
echo "Results dir: $OUT (N=$N MMR entries)"

MMR_SRC="org/minima/objects/mmr/m_m_r.cpp org/minima/objects/mmr/m_m_r_data.cpp org/minima/objects/mmr/m_m_r_entry.cpp org/minima/objects/mmr/m_m_r_entry_number.cpp org/minima/objects/mmr/m_m_r_proof.cpp org/minima/objects/base/mini_data.cpp org/minima/objects/base/mini_number.cpp org/minima/objects/base/mini_byte.cpp org/minima/objects/base/mini_string.cpp org/minima/utils/crypto.cpp org/minima/utils/streamable.cpp org/minima/utils/fast_byte_array_stream.cpp org/minima/utils/base_converter.cpp org/minima/system/params/general_params.cpp org/minima/system/params/global_params.cpp org/minima/utils/json/j_s_o_n_aware.cpp org/minima/utils/json/j_s_o_n_array.cpp org/minima/utils/json/j_s_o_n_object.cpp org/minima/utils/json/j_s_o_n_stream_aware.cpp org/minima/utils/json/j_s_o_n_value.cpp org/minima/utils/json/j_s_o_n_writer.cpp org/minima/utils/json/item_list.cpp org/minima/utils/json/parser/container_factory.cpp org/minima/utils/json/parser/content_handler.cpp org/minima/utils/json/parser/j_s_o_n_parser.cpp org/minima/utils/json/parser/parse_exception.cpp org/minima/utils/json/parser/yylex.cpp org/minima/utils/json/parser/yytoken.cpp tests/stubs.cpp"

echo "== C++ =="
( cd "$PROJECT_DIR" && $CXX -std=c++17 -O3 -DNDEBUG -I. $OPENSSL_INCLUDE \
    -o /tmp/bench_ram "$_BENCH_DIR/cpp/bench_ram.cpp" $MMR_SRC \
    $OPENSSL_LIB -lssl -lcrypto -lpthread )
/tmp/bench_ram "$N" | tee "$OUT/cpp_ram.txt"

echo "== Java =="
sh "$HERE/build_minima_core.sh" >/dev/null 2>&1
CP=$(ls "$MINIMA_CORE"/lib/*.jar | tr '\n' ':')"$MC_BUILD"
"$JDK/javac" -cp "$CP" -d "$MC_BENCH_OUT" "$_BENCH_DIR/java/BenchRam.java"
"$JDK/java" -Xmx4g -cp "$CP:$MC_BENCH_OUT" BenchRam "$N" > "$OUT/java_ram.txt" 2>&1 &
jp=$!
peak=0
while kill -0 "$jp" 2>/dev/null; do
    r=$(ps -o rss= -p "$jp" 2>/dev/null | tr -d ' ')
    [ -n "$r" ] && [ "$r" -gt "$peak" ] 2>/dev/null && peak=$r
    sleep 0.1
done
wait "$jp" 2>/dev/null || true
echo "java_peak_rss_kb	$peak	kB" >> "$OUT/java_ram.txt"
cat "$OUT/java_ram.txt"

cab() { grep -E "^$1\b" "$OUT/cpp_ram.txt" | head -1 | cut -f2; }
jab() { grep -E "^$1\b" "$OUT/java_ram.txt" | head -1 | cut -f2; }
{
  echo "# RAM-scaling comparison (MMR, N=$N entries)"
  echo
  echo "C++ = this node | Java = minima-core \`$(git -C "$MINIMA_CORE" rev-parse --short HEAD)\` (performance reference only)."
  echo
  echo "| Metric | C++ (this node) | Java (minima-core) |"
  echo "|---|---|---|"
  echo "| bytes/MMR entry | $(cab mmr_bytes_per_entry) | $(jab mmr_bytes_per_entry) (heap) |"
  echo "| delta for $N entries (kB) | $(cab mmr_delta_kb) | $(jab heap_delta_kb) (heap) |"
  echo "| process peak RSS (kB) | $(cab rss_after_mmr_kb) | $(jab java_peak_rss_kb) |"
  echo
  echo "Caveats: C++ is process RSS; Java rows are JVM heap-in-use (plus the peak"
  echo "RSS row which is the whole JVM). Heap != RSS. Single run, indicative only."
} | tee "$OUT/RAM_SUMMARY.md"

echo "DONE -> $OUT"
