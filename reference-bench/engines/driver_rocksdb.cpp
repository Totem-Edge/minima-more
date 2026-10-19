// RFC-0003 — RocksDB driver (role R1 archive + R2 coin store).
//
// STATUS: scaffold. RocksDB is not installed on the dev host and adds a heavy
// dependency; the plan (RFC-0003 §2) is to build it on the A1 lab host alongside
// the other engines. Until then these factories report the engine as unavailable
// (return nullptr) so the runner skips it cleanly.
//
// Implementation notes for when `find rocksdb` succeeds:
//  - Archive: default CF `blocks` keyed by 8-byte BE height; second CF `ididx`
//    (txpowid -> height). Bulk load via a single WriteBatch (or WriteOptions
//    {disableWAL=false, sync=false}) to match the other engines' bulk discipline.
//  - Coin: default CF `coins` keyed by coinid; CF `addridx` keyed by
//    addr||coinid for the prefix scan; iteration via `NewIterator` + Seek.
//  - snapshot: `DB::Open` on a checkpoint dir (`Checkpoint::CreateCheckpoint`)
//    or `DB::GetSnapshot` + manual copy.
#include "interface.hpp"

namespace eng {

std::unique_ptr<ArchiveStore> make_rocksdb_archive() { return nullptr; }
std::unique_ptr<CoinStore> make_rocksdb_coin() { return nullptr; }

}  // namespace eng
