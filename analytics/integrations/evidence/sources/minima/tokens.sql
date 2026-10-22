select
  a.token_id,
  coalesce(m.name, a.token_id) as name,
  m.decimals,
  coalesce(m.is_nft, false) as is_nft,
  a.coin_count,
  a.total_raw,
  a.holder_count,
  m.image_url
from analytics.asset_snapshots a
left join analytics.token_metadata m on m.token_id = a.token_id
where a.snapshot_day_ms = (select max(snapshot_day_ms) from analytics.asset_snapshots)
order by a.total_raw desc
