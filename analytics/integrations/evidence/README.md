# Minima More — Evidence

MIT-licensed, static/self-hosted BI for the analytics index.

```bash
cd analytics/integrations/evidence
npm install
ln -s /path/to/analytics.duckdb sources/minima/analytics.duckdb
npm run sources
npm run dev
```

`connection.yaml` reads `sources/minima/analytics.duckdb` (relative to the
source directory). Evidence extracts the source queries to Parquet at build
time, then serves the pages.

Pages: overview (`/`), tokens/NFTs (`/tokens`), scripts/magic (`/scripts`).
The database is not committed. For hosted deployments, use Evidence Studio or
self-host the built static site; Evidence Studio/embedded features are paid.
