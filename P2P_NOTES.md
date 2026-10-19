# P2P / Peer-Discovery Notes

Running record of the peer-to-peer connectivity work on Minima More. Distinguishes
"local networking" fixes (no consensus/wire-format impact) from protocol changes.

Status vs the Java reference: this node was **well behind** on P2P — it held a single
bootstrap connection and never discovered peers. The fixes below close the main gap;
a few refinements remain.

## Symptom (before)
- `network` showed `connected:1`, `numKnownPeers:0`, `numAllLinks:0`.
- Log looped `No default Peers found - please use peer command…`.
- Java nodes fan out to many peers; this node stayed at one.

## Root cause
1. **`sendPingMessage()` framing bug.** It sent the raw `MiniData` payload over a raw
   socket **without** the NIO 4-byte big-endian length prefix. The normal write path
   (`NIOClient`) prepends `putInt32BE(len)`. Peers therefore read the leading payload
   bytes as a bogus length and closed the connection (`recv length failed`). This
   silently broke *all* discovery (and peer verification).
2. **Discovery was inert.** `doDiscoveryPing()` returned immediately when the
   compiled-in `DEFAULT_NODE_LIST` was empty (always in this build).
3. **No seed source.** No compiled seeds; the v1.0.x peers' NIO greetings carry no
   peer list, so there was nothing to learn from and no default peers to try.
4. **Checker not fed.** The path that actually connects (the P2P **PeersChecker**) was
   never handed the seeds.

## Fixes (applied)
| # | Change | Commit |
|---|---|---|
| 1 | `SINGLE_PING` now carries the 4-byte length prefix (matches the normal write path) | `fc0e346` |
| 2 | `doDiscoveryPing()` seeds from: compiled list → configured `-p2pnodes` (minus already-linked) → **baked-in peer-list URLs**, merged | `fc0e346`, `d242792` |
| 3 | Baked-in URLs: `https://eurobuddha.com/minimapeers.txt` and `https://spartacusrex.com/minimapeers.txt` | `d242792` |
| 4 | Submit seeds to the PeersChecker as **`PEERS_CHECKPEERS` (force)** — the path that verifies + connects (`ADDPEERS` is gated on an existing link) | `d242792` |
| 5 | Consume a peer's advertised peer list from its NIO greeting (when present) | `fc0e346` |
| 6 | Don't blacklist a real mainnet peer on "no crossover" when the node is tip-less | `92a4e73` |

## Verified (OCI A1)
- Node with **no `-p2pnodes`**: reached **~10 connections / 12 known peers** (was 1 / 0).
- Node with 3 configured peers: **3 connections / 3 known**.
- Peer persistence works: `p2p2.db` written on the ~5-min timer / clean shutdown and
  loaded at startup (note: a hard kill skips the last save).

## Remaining refinements
- **(a) Link maintenance** — target link count, reconnect backoff, liveness table
  (skip recently-failed hosts; the URL lists contain many dead entries), prefer
  recently-seen peers. Medium effort, low risk.
- **(b) Async/parallel verification** — `checkUnverifiedPeer` pings inline; a 10 s
  timeout per dead peer makes cold-start verification slow. Parallelize. Medium
  effort, low risk.
- **(c) Full P2P swap-links gossip** — learn peers from *connected* peers over the
  existing link (the proper Minima mechanism and true Java parity). Larger effort.

## Risk / interop
All of the above is **local connection management**. It reuses the **existing** Minima
NIO/P2P message types and does **not** touch consensus, block/TxPoW validation,
hashing, or chain serialization — so **existing mainnet nodes are unaffected**. The
only failure mode of a bug here is degraded connectivity on our side.
