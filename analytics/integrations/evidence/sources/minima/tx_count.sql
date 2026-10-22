select
  to_timestamp(bucket_start_ms / 1000) as day,
  height,
  value as transactions
from analytics.metric_series
where metric = 'tx_count' and granularity = '1d'
order by bucket_start_ms
