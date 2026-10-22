select
  to_timestamp(bucket_start_ms / 1000) as day,
  height,
  value_raw as burned_raw,
  value as burn_count
from analytics.metric_series
where metric in ('burn_raw', 'burn_count') and granularity = '1d'
order by bucket_start_ms
