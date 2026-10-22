with tip as (
  select snapshot_day_ms
  from analytics.asset_age_bands
  where token_id = '0x00'
  order by snapshot_day_ms desc
  limit 1
)
select
  band,
  coin_count,
  amount_raw,
  amount_raw / sum(amount_raw) over () as share
from analytics.asset_age_bands, tip
where token_id = '0x00' and asset_age_bands.snapshot_day_ms = tip.snapshot_day_ms
order by band
