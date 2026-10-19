# Live-mainnet functional test log

Functional tests of the C++ node (`Minima More`, version `1.0.100.0`) against the
**live Minima mainnet**, run on the OCI Ampere A1 lab box. Distinct from the
component benchmarks (`RESULTS_LOG.md`) — these exercise the node end to end on
the real network.

Node setup: `-daemon -allowallip -p2pnodes <reachable peers>`, RPC on localhost,
`setsid`-detached, persistent data dir.

---

## Sync to mainnet tip

- Bootstrapped from live 1.0.4x peers; received the peer `GREETING` every attempt.
- `Initial Block Download received` (~5 MB / ~1070 blocks) → cascade loaded →
  synced to the **live tip** (block ~2,373,3xx–2,373,9xx during the session).
- Stayed connected and followed new blocks (tip advanced block-by-block).

## Receive (wallet funding)

- Sent a real microspend (0.01 MINIMA) to the node's address; the node saw it as
  `unconfirmed` then `confirmed`/`sendable` within a few blocks.
- `coins` shows the coin with `coinid`, `address`, `mmrentry`, `created`, `age`.
- `history` returned the **full TxPoW** for the incoming tx — header, body,
  WOTS witness signatures and MMR proofs all parsed/validated (i.e. real mainnet
  transaction validation works).

## KISSVM

- `runscript script:"RETURN TRUE"` → parsed successfully and produced a script
  address. (The script value must be quoted, since it contains spaces.)

## Send (build / sign / mine / broadcast)

- `send address:<addr> amount:0.005` built a TxPoW: correct `inputs` (the funding
  coin), `outputs` (payment + change), WOTS `witness.signatures` and MMR proofs,
  and a mined header. Returned `txpowid 0x14029EA4…`; the tx then appeared in the
  **mempool** (`mempool: 1`).
- **Round-trip to an external wallet:** sent 0.005 MINIMA to an external
  miniaddress (`send address:MxG08… amount:0.005`, `txpowid 0x07AC33F1…`). It
  confirmed at block 2,374,003 and the node's balance dropped 0.01 -> 0.005 —
  a real outbound mainnet payment accepted by the network.

## Token creation

- `tokencreate name:MPATest amount:1000` built/signed/mined a token TxPoW
  (`txpowid 0xBAF52C55…`, block 2,373,991). On confirmation the new token
  materialised: `MPATest`, `tokenid 0xE0514E13CB180C133CC24DC3944AE53F9F8CFCB5A336C5641E513793A9350BBE`,
  total 1000, balance `unconfirmed 1000 -> confirmed/sendable 1000`. 0.005 MINIMA
  was melted into it (expected Minima tokenomics), 0.005 change returned.

## Live-mainnet benchmark (C++ vs Java)

See `results/a1-mainnet-bench/` and the 2026-10-19 entry in `RESULTS_LOG.md`:
fresh C++ and Java nodes syncing the real chain — C++ peak-sync RSS ~92 MB vs
~407 MB, idle RSS ~210 MB vs ~1.39 GB, CPU-seconds (60 s settle) ~34 vs ~68,
threads 25 vs 60, data dir ~220 MB vs ~881 MB.

## Commands exercised

`status`, `block`, `getaddress`, `balance`, `coins`, `tokens`, `keys`, `history`,
`peers`, `network`, `vault`, `megammr`, `runscript`, `send`.

## Findings from mainnet testing

- **Peer IBD rate limit (~30 min).** Live peers enforce `LIMIT_IBD_30MINS`, keyed
  on the node's advertised address: the same `host:port` is served an IBD once,
  then the peer keeps sending its greeting but withholds the IBD. A **fresh port
  per run → 3/3 syncs**; reuse of one port → 0–1/4. `run_mainnet_sync.sh` /
  `run_mainnet_bench.sh` therefore pick a unique port per run.
- **No-crossover blacklist guard** (fixed): a fresh node no longer marks a valid
  mainnet peer "different chain" when its own tree is empty.
- Wire frames (greeting/IBD/TXPOW) match the Java reference client.

Reproduce the read-only subset with `reference-bench/scripts/run_mainnet_commands.sh`.
