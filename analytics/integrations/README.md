# Analytics BI integrations

Both templates consume the same `analytics.duckdb` produced by
`minima-analytics -index`. They are optional and never part of the node build.

- **Evidence** — recommended default for **history**: MIT, static/self-hosted,
  DuckDB-native; HODL waves, CDD, cohorts, tokens, scripts.
- **Rill** — Apache-2.0 for **live/operational** dashboards: tip, lag, recent
  blocks, burn, magic changes, script activity, alerts; attaches the DuckDB file
  as an external database.
- **Shared contract:** `common/metrics.yaml` defines metric ownership and
  refresh semantics. Do not duplicate all pages across platforms.

## Shared preparation

```bash
# Build or restore the index first
./minima-analytics -data ~/.minima -indexdb ~/.minima/analytics.duckdb -index
```

Copy or symlink `analytics.duckdb` into each template as described in its
README. Do not commit the database; it can be several GB.

## Security

These templates expose read-only analytics. Do not expose Rill/Evidence
deployments publicly without auth; neither tool touches wallet keys.
