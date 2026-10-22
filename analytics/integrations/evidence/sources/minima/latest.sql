select status, last_height, last_hash, lag_blocks, updated_at_ms
from analytics.index_state
where id = 1
