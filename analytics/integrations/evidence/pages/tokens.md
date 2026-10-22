---
title: Tokens & NFTs
queries:
  - tokens: minima/tokens.sql
---

# Tokens & NFTs

<BigValue data={tokens} value=token_id title="Tokens" />
<DataTable data={tokens} rows=25 search=true>
  <Column id=name title="Name" />
  <Column id=decimals title="Decimals" />
  <Column id=is_nft title="NFT" />
  <Column id=coin_count title="Coins" />
  <Column id=holder_count title="Holders" />
  <Column id=total_raw title="Supply (raw)" />
</DataTable>
