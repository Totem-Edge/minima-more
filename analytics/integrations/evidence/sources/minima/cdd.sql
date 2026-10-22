select
  to_timestamp(bucket_start_ms / 1000) as day,
  metric,
  value_raw
from analytics.metric_series
where metric in ('cdd', 'cdd_90', 'cyd') and granularity = '1d'
order by bucket_start_ms, metric
