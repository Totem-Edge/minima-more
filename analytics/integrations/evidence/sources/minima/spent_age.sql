select
  to_timestamp(bucket_start_ms / 1000) as day,
  metric,
  value
from analytics.metric_series
where metric in ('spent_output_age_avg_days', 'spent_output_age_median_days', 'dormancy_days')
  and granularity = '1d'
order by bucket_start_ms, metric
