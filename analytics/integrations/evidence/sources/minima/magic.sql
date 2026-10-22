select height, block_hash, max_txpow_size, max_kissvm_ops, max_txns, min_work
from analytics.magic_snapshots
order by height desc
limit 1
