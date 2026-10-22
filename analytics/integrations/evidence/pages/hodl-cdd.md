---
title: HODL waves & coin age
queries:
  - hodl_waves: minima/hodl_waves.sql
  - cdd: minima/cdd.sql
  - spent_age: minima/spent_age.sql
---

# HODL waves & coin age

<BarChart data={hodl_waves} x=band y=share yAxisTitle="Share of supply" />

<DataTable data={hodl_waves} />

## Coin days destroyed

<LineChart data={cdd} x=day y=value_raw series=metric yAxisTitle="CDD (raw)" />

## Spent-output lifespan / dormancy

<LineChart data={spent_age} x=day y=value series=metric yAxisTitle="Days" />
