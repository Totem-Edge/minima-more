select
  to_timestamp(bucket_start_ms / 1000) as day,
  height,
  value as blocks
from analytics.metric_series
where metric = 'block_count' and granularity = '1d'
order by bucket_start_ms
