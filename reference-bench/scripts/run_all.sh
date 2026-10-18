#!/bin/sh
# Full reference-bench run: capture env, run the Java minima-core harness and the
# C++ harnesses, and write a SUMMARY.md. Output goes to results/<timestamp>-<arch>/.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/_env.sh"

OUT=${1:-$_BENCH_DIR/results/$(date +%Y%m%d-%H%M%S)-$(uname -m)}
mkdir -p "$OUT"
echo "Results dir: $OUT"

# ---- environment / provenance ----
{
  echo "# Environment / provenance"
  echo "date_utc: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "host: $(uname -a)"
  echo "arch: $(uname -m)"
  echo "cpu: $(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo unknown)"
  echo "cores: $(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo unknown)"
  echo "mem_bytes: $(sysctl -n hw.memsize 2>/dev/null || echo unknown)"
  echo "cc: $(c++ --version | head -1)"
  echo "java: $("$JDK/java" -version 2>&1 | head -1)"
  echo "minima_core: $MINIMA_CORE"
  echo "minima_core_commit: $(git -C "$MINIMA_CORE" rev-parse HEAD 2>/dev/null || echo missing)"
  echo "minima_core_commit_date: $(git -C "$MINIMA_CORE" show -s --format=%ci HEAD 2>/dev/null || echo missing)"
} | tee "$OUT/environment.txt"

# ---- Java reference ----
echo "== Java (minima-core) =="
sh "$HERE/build_minima_core.sh"
CP=$(ls "$MINIMA_CORE"/lib/*.jar | tr '\n' ':')"$MC_BUILD"
"$JDK/java" -cp "$CP:$MC_BENCH_OUT" BenchMinima 200 20000 500000 | tee "$OUT/java_bench.txt"

# ---- C++ ----
COMMON="org/minima/utils/crypto.cpp org/minima/objects/base/mini_data.cpp org/minima/objects/base/mini_number.cpp org/minima/objects/base/mini_byte.cpp org/minima/objects/base/mini_string.cpp org/minima/utils/streamable.cpp org/minima/utils/fast_byte_array_stream.cpp org/minima/utils/base_converter.cpp org/minima/system/params/general_params.cpp org/minima/system/params/global_params.cpp org/minima/utils/json/j_s_o_n_aware.cpp org/minima/utils/json/j_s_o_n_array.cpp org/minima/utils/json/j_s_o_n_object.cpp org/minima/utils/json/j_s_o_n_stream_aware.cpp org/minima/utils/json/j_s_o_n_value.cpp org/minima/utils/json/j_s_o_n_writer.cpp org/minima/utils/json/item_list.cpp org/minima/utils/json/parser/container_factory.cpp org/minima/utils/json/parser/content_handler.cpp org/minima/utils/json/parser/j_s_o_n_parser.cpp org/minima/utils/json/parser/parse_exception.cpp org/minima/utils/json/parser/yylex.cpp org/minima/utils/json/parser/yytoken.cpp tests/stubs.cpp"

echo "== C++ sha3 =="
( cd "$PROJECT_DIR" && $CXX -std=c++17 -O3 -DNDEBUG -I. $OPENSSL_INCLUDE \
    -o /tmp/bench_sha3 "$_BENCH_DIR/cpp/bench_sha3.cpp" $COMMON \
    $OPENSSL_LIB -lssl -lcrypto -lpthread )
/tmp/bench_sha3 500000 | tee "$OUT/cpp_sha3.txt"

echo "== C++ WOTS signature =="
( cd "$PROJECT_DIR" && $CXX -std=c++17 -O3 -DNDEBUG -I. $OPENSSL_INCLUDE \
    -o /tmp/bench_wots_sig tests/mshs2/bench/bench_wots_signature.cpp \
    org/minima/objects/keys/winternitz.cpp $COMMON \
    $OPENSSL_LIB -lssl -lcrypto -lpthread )
/tmp/bench_wots_sig 500 | tee "$OUT/cpp_wots_signature.txt"

echo "== C++ bench_core (mirror) =="
COMMON_FULL="$COMMON org/minima/database/txpowdb/ram/ram_d_b.cpp org/minima/database/txpowdb/ram/ram_data.cpp org/minima/objects/tx_po_w.cpp org/minima/objects/tx_body.cpp org/minima/objects/tx_header.cpp org/minima/objects/transaction.cpp org/minima/objects/witness.cpp org/minima/objects/coin.cpp org/minima/objects/coin_proof.cpp org/minima/objects/token.cpp org/minima/objects/script_proof.cpp org/minima/objects/magic.cpp org/minima/objects/state_variable.cpp org/minima/objects/address.cpp org/minima/objects/keys/signature.cpp org/minima/objects/keys/signature_proof.cpp org/minima/objects/keys/winternitz.cpp org/minima/objects/keys/tree_key.cpp org/minima/objects/keys/tree_key_node.cpp org/minima/objects/mmr/m_m_r_proof.cpp org/minima/objects/mmr/m_m_r.cpp org/minima/objects/mmr/m_m_r_data.cpp org/minima/objects/mmr/m_m_r_entry.cpp org/minima/objects/mmr/m_m_r_entry_number.cpp"
( cd "$PROJECT_DIR" && $CXX -std=c++17 -O3 -DNDEBUG -I. $OPENSSL_INCLUDE \
    -o /tmp/bench_core "$_BENCH_DIR/cpp/bench_core.cpp" $COMMON_FULL \
    $OPENSSL_LIB -lssl -lcrypto -lpthread )
/tmp/bench_core 200 20000 500000 | tee "$OUT/cpp_bench_core.txt"

# ---- summary ----
{
  echo "# Reference benchmark summary"
  echo
  echo "Run: $OUT"
  echo "Java: minima-core \`$(git -C "$MINIMA_CORE" rev-parse --short HEAD)\` (performance reference only — NOT byte-parity)."
  echo
  jv() { grep -E "^$1\b" "$OUT/java_bench.txt" | head -1 | cut -f2; }
  cv() { grep -E "^$1\b" "$OUT/cpp_bench_core.txt" | head -1 | cut -f2; }
  row() { j=$(jv "$1"); c=$(cv "$1"); awk -v m="$2" -v j="$j" -v c="$c" 'BEGIN{ printf "| %s | %s | %s |\n", m, j, c }'; }
  echo "| Metric | Java (minima-core) | C++ (this node) |"
  echo "|---|---|---|"
  row wots_pubkey_ms "WOTS pubkey (ms/op)"
  row wots_sign_ms "WOTS sign (ms/op, batched)"
  row wots_verify_ms "WOTS verify (ms/op)"
  row sha3_224_hashes_per_sec "SHA3-256 224B (h/s, 1 thr)"
  row mmr_add_entries_per_sec "MMR addEntry+getRoot (entries/s)"
  echo
  echo "C++ WOTS sign scalar vs batched:"
  grep -E '^(scalar|batched|speedup|identical)' "$OUT/cpp_wots_signature.txt" | sed 's/^/    /'
  echo
  echo "Raw files: environment.txt, java_bench.txt, cpp_sha3.txt, cpp_wots_signature.txt, cpp_bench_core.txt"
} | tee "$OUT/SUMMARY.md"

echo "DONE -> $OUT"
