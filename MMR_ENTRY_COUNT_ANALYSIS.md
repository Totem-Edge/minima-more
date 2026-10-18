# MMR entry count — analysis and safe levers

Status: **analysis only.** Follows the MMR framing work (858 → 698 B/entry).
Framing is a constant factor; entry count is the dominant variable. This note
records what consumes entries and — importantly — why the obvious "prune it"
lever is **not** safe in the consensus MMR.

## How entries are created (`TxPoWTreeNode::constructMMR`)

Each block node builds its own MMR from the block:
1. **Previous peaks** from `TxBlock::getPreviousPeaks()` are `setEntry(...)`-ed
   (one per MMR peak; peak count = popcount(UTXO) ≈ ~10–20 for mainnet scale).
2. **Spent inputs** are `updateEntry(...)`-ed with `value = ZERO` (the leaf stays,
   marked spent via a zero value).
3. **New outputs** are `addEntry(...)`-ed (value = amount).

The tree retains blocks back to `MINIMA_CASCADE_START_DEPTH = 2048`
(`tx_po_w_processor.cpp` `setLength(...)`), while proofs only need
`MINIMA_MMR_PROOF_HISTORY = 256`. `MINIMA_CASCADE_LEVELS = 32`,
`MINIMA_CASCADE_LEVEL_NODES = 256`.

So RAM ≈ (UTXO leaves) + (spent-to-zero leaves still inside the 2048-block
window) + (peaks-per-node × retained nodes), all at the framing cost.

## Why "prune" is not a safe lever here

`MMR::pruneTree()` / `MMR::scanUnspendableTree()` remove zero-value (all-spent)
subtrees from `mSetEntries`. That is correct for the **MegaMMR** (an auxiliary,
off-consensus store — it is only invoked there, `tx_po_w_processor.cpp:659`,
gated by `MEGAMMR_MEGAPRUNE*`). It is **unsafe for the consensus MMR**:

- The MMR root / peaks are consensus (referenced by block headers and proofs).
  Removing entries changes `getPeaks()`/`getRoot()` → a **different root** →
  consensus break.
- Zero-value leaves are not the only zero-valued state: state-carrying outputs
  are meaningful too, and the mega-prune flags (`megaprunestate`,
  `megaprunetokens`) exist precisely because pruning them is a deliberate,
  opt-in trade-off for exchange/archive nodes, not a default.

Therefore there is **no consensus-neutral way to drop entries** from the
in-memory MMR. The entry count is intrinsic to the UTXO set + the retained
block window.

## Legitimate levers (all opt-in / parameter, not code risks)

1. **MegaMMR offload** (`-megammr`, plus `-megaprune`, `-megaprunestate`,
   `-megaprunetokens`): move the historical MMR to the DB-backed MegaMMR and keep
   the RAM MMR bounded. This is the intended mechanism for memory-constrained
   devices. Trade-off: disk lookups / no historical proofs for pruned coins.
2. **Retained-window tuning**: `MINIMA_CASCADE_START_DEPTH` (2048) is 8× the
   proof history (256). If reorg policy allows, lowering it reduces
   peaks-per-node × nodes and spent-to-zero retention. This is a network/
   consensus parameter — **do not change unilaterally**; it must match the
   network's reorg expectations.
3. **Framing** (already done): 858 → 698 B/entry.

## Recommendation

- **Do not** add default pruning to the consensus MMR — it would fork the chain.
- For low-RAM deployments, document and use **MegaMMR mode** (offload + optional
  prune flags) rather than changing defaults.
- The retained window (2048 vs 256) is worth raising with the network/consensus
  owners as a *parameter* discussion, not a code change here.

Net: entry-count reduction is a deployment/mode decision (MegaMMR) or a network
parameter change, not a safe local optimization. The framing win (step 1–4) is
the safe part and is complete.
