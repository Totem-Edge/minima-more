---
title: Minima More
queries:
  - latest: minima/latest.sql
  - block_count: minima/block_count.sql
  - tx_count: minima/tx_count.sql
  - supply: minima/supply.sql
  - utxo_age: minima/utxo_age.sql
---

# Minima More analytics

<BigValue data={latest} value=last_height title="Block height" />
<BigValue data={latest} value=lag_blocks title="Index lag" />
<BigValue data={supply} value=supply_raw title="Supply (raw)" />

## Blocks per day

<LineChart data={block_count} x=day y=blocks yAxisTitle="Blocks" />

## Transactions per day

<LineChart data={tx_count} x=day y=transactions yAxisTitle="Transactions" />

## Native UTXO age bands

<BarChart data={utxo_age} x=band y=amount_raw yAxisTitle="Amount (raw)" />
