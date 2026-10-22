select
  to_timestamp(bucket_start_ms / 1000) as day,
  height,
  value_raw as supply_raw
from analytics.metric_series
where metric = 'supply_raw' and granularity = 'snapshot'
order by bucket_start_ms
