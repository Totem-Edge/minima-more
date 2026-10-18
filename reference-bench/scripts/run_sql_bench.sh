#!/bin/sh
# SQLite write-throughput harness: TxPoWSqlDB inserts batched (one transaction
# per BATCH_MAX_ROWS) vs one-transaction-per-row (the old behaviour, toggled via
# TxPoWSqlDB::DISABLE_BATCH). TxPoWs arrive in bursts during sync/tree-load/
# restore, so this is the device-relevant DB write cost.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/_env.sh"

N=${N:-50000}
OUT=${1:-$_BENCH_DIR/results/$(date +%Y%m%d-%H%M%S)-$(uname -m)}
mkdir -p "$OUT"
echo "Results dir: $OUT (N=$N TxPoWs)"

SQL_SRC="org/minima/database/txpowdb/sql/tx_po_w_sql_d_b.cpp org/minima/utils/sql_d_b.cpp org/minima/database/txpowdb/ram/ram_d_b.cpp org/minima/database/txpowdb/ram/ram_data.cpp org/minima/objects/tx_po_w.cpp org/minima/objects/tx_body.cpp org/minima/objects/tx_header.cpp org/minima/objects/transaction.cpp org/minima/objects/witness.cpp org/minima/objects/coin.cpp org/minima/objects/coin_proof.cpp org/minima/objects/token.cpp org/minima/objects/script_proof.cpp org/minima/objects/magic.cpp org/minima/objects/state_variable.cpp org/minima/objects/address.cpp org/minima/objects/keys/signature.cpp org/minima/objects/keys/signature_proof.cpp org/minima/objects/keys/winternitz.cpp org/minima/objects/keys/tree_key.cpp org/minima/objects/keys/tree_key_node.cpp org/minima/objects/mmr/m_m_r_proof.cpp org/minima/objects/mmr/m_m_r.cpp org/minima/objects/mmr/m_m_r_data.cpp org/minima/objects/mmr/m_m_r_entry.cpp org/minima/objects/mmr/m_m_r_entry_number.cpp org/minima/objects/base/mini_data.cpp org/minima/objects/base/mini_number.cpp org/minima/objects/base/mini_byte.cpp org/minima/objects/base/mini_string.cpp org/minima/utils/crypto.cpp org/minima/utils/streamable.cpp org/minima/utils/fast_byte_array_stream.cpp org/minima/utils/base_converter.cpp org/minima/system/params/general_params.cpp org/minima/system/params/global_params.cpp org/minima/utils/json/j_s_o_n_aware.cpp org/minima/utils/json/j_s_o_n_array.cpp org/minima/utils/json/j_s_o_n_object.cpp org/minima/utils/json/j_s_o_n_stream_aware.cpp org/minima/utils/json/j_s_o_n_value.cpp org/minima/utils/json/j_s_o_n_writer.cpp org/minima/utils/json/item_list.cpp org/minima/utils/json/parser/container_factory.cpp org/minima/utils/json/parser/content_handler.cpp org/minima/utils/json/parser/j_s_o_n_parser.cpp org/minima/utils/json/parser/parse_exception.cpp org/minima/utils/json/parser/yylex.cpp org/minima/utils/json/parser/yytoken.cpp tests/stubs.cpp"

echo "== C++ =="
( cd "$PROJECT_DIR" && $CXX -std=c++17 -O3 -DNDEBUG -I. $OPENSSL_INCLUDE \
    -o /tmp/bench_sql_batch "$_BENCH_DIR/cpp/bench_sqlbatch.cpp" $SQL_SRC \
    $OPENSSL_LIB $SQLITE_LIB -lssl -lcrypto -lpthread )
/tmp/bench_sql_batch "$N" | tee "$OUT/cpp_sqlbatch.txt"

cab() { grep -E "^$1\b" "$OUT/cpp_sqlbatch.txt" | head -1 | cut -f2; }
{
  echo "# TxPoWSqlDB write throughput (N=$N TxPoWs)"
  echo
  echo "| | time | inserts/s |"
  echo "|---|---|---|"
  echo "| one txn per row (old) | $(cab unbatched_ms) ms | $(cab unbatched_inserts_per_sec) |"
  echo "| batched (new) | $(cab batched_ms) ms | $(cab batched_inserts_per_sec) |"
  echo
  echo "Speedup: **$(cab speedup)x** (TxPoW build time excluded; both paths insert the same objects)."
} | tee "$OUT/SQL_SUMMARY.md"

echo "DONE -> $OUT"
