---
title: Scripts
queries:
  - scripts: minima/scripts.sql
  - magic: minima/magic.sql
---

# Scripts & network magic

<DataTable data={magic} />

<DataTable data={scripts} rows=50 search=true>
  <Column id=script_hash title="Script" />
  <Column id=name title="Known name" />
  <Column id=category title="Category" />
  <Column id=output_count title="Outputs" />
  <Column id=spent_count title="Spent" />
  <Column id=unspent_value_raw title="Unspent (raw)" />
</DataTable>
