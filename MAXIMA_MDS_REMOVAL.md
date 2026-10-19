# Maxima / MDS removal — inventory, safety analysis, verification

**Status:** C++ port (Minima More) has already removed the Maxima/MDS surface.
This document (a) inventories what those layers were wired into in the Java
reference, (b) records exactly what the port dropped, (c) identifies the **shared**
subsystems that must be preserved, and (d) gives a node-ops verification checklist.

Reference: `minima-core` `c2a54a2`; Minima More `main`.

---

## 1. What Maxima / MDS are

- **Maxima** — the P2P messaging / identity layer. Encrypted point-to-point payloads
  (`EncryptDecrypt`) carried as `Magic` objects over the same NIO transport as the
  Minima protocol; PoW-secured (mined like TxPoW). Lets users "transmit any data
  they like, not just Minima transactions" (see `whitepaper`).
- **MDS** — Mini Data Storage / public-MDS: a decentralised data layer (files/blobs),
  with its own flags, folder, password and SSL options.
- Both carry **arbitrary user blobs** — the reason the inherited `MiniData` cap is a
  large 512 MB (a consensus node never needs this).

## 2. Java wiring inventory (what the layers touched)

| Area | Files | What it did |
|---|---|---|
| Params / CLI flags | `system/params/GeneralParams.java`, `ParamConfigurer.java` | `MDS_ENABLED`, `MDS_PASSWORD`, `MDS_INITFOLDER`, `MDS_WRITE`, `MDS_NOSSL`, `PUBLICMDS_ENABLE`, `PUBLICMDS_SESSION_UID`; `-mds`, `-mdsinitfolder`, `-publicmds`, `-mdsnosl` |
| Database | `database/MinimaDB.java`, `database/userprefs/UserDB.java` | MDS enable/host flag; MDS data alongside core DB |
| Objects | `objects/Magic.java` | the Maxima message packet |
| Crypto | `utils/encrypt/EncryptDecrypt.java`, SSL/keystore | encrypted Maxima payloads; MDS SSL |
| Network | `network/minima/NIOClient.java`, `NIOManager.java`, `NIOMessage.java` (incl. `MDS_HEAVIER_CHAIN` notify), `network/p2p/P2PManager.java` | Maxima/MDS messages ride the **same NIO/NIOMessage** transport |
| Node lifecycle | `Minima.java`, `system/Main.java` | start/stop MDS + Maxima services |
| Command surface | `commands/CommandRunner.java`, `commands/base/healthcheck|logs|trace.java`, `commands/whitepaper.java`, `network/rpc/CMDHandler.java` | Maxima/MDS commands + RPC |
| Mining | `system/brains/TxPoWMiner.java` | Maxima message PoW |

## 3. What Minima More removed

The port dropped the Maxima/MDS surface: **NIO-level Maxima message types, MDS/SSL
params, userprefs flags, and the associated command list** (see `OPTIMIZATION_BACKLOG.md`,
"Maxima/MDS surface removed"). `Assembly`/`Main` no longer start MDS/Maxima.

**Residuals (cosmetic only, no functional effect):**
- `org/minima/system/commands/whitepaper.cpp` — whitepaper prose that *describes*
  Maxima (documentation, not code).
- `org/minima/system/commands/base/logs.cpp` — a log/help string.

## 4. Shared subsystems — DO NOT remove these

Removal is only *safe* because these are kept (Maxima used them, but the core node
also needs them):

- **NIO transport** (`NIOClient/NIOManager/NIOMessage`) — carries GREETING/IBD/TXPOW,
  not just Maxima. Keep; drop only Maxima-specific message *types*.
- **P2PManager** — core peer discovery/links. Keep.
- **SSL keystore** — confirm whether anything *other* than MDS/public-MDS needs it;
  if not, RPC/SSL hardening can be simplified, but don't break RPC TLS if used.
- **MinimaDB/UserDB** — core DB; remove only the MDS *flag/columns*, not the DB.
- **TxPoW mining** — Minima PoW; keep. Only Maxima-message mining is dropped.
- **EncryptDecrypt** — if unused elsewhere, can go; verify no core path uses it.

## 5. Safe-removal method (for the Java reference, and as a checklist for the port)

1. **Remove message types, not the transport.** Delete Maxima/MDS `NIOMessage`
   `type` handlers and their dispatch cases; leave NIO/IGB/TXPOW intact.
2. **Params**: drop MDS/PUBLICMDS flags and their parsing; ensure no core path reads
   the removed symbols (compiler is the backstop).
3. **DB**: remove MDS flag columns/usages; never drop core tables; guard migrations.
4. **Lifecycle**: ensure startup/shutdown no longer reference MDS/Maxima services.
5. **Commands/RPC**: remove Maxima/MDS commands from the command registry so they
   aren't reachable; keep the dispatcher.
6. **Mining**: ensure removing Maxima PoW does not change Minima mining state.
7. **Crypto**: keep `EncryptDecrypt`/SSL only if a retained path needs them.

## 6. Verification checklist (node ops unchanged)

Run after any further cleanup:

- [ ] Node builds clean; no warnings.
- [ ] Genesis/solo smoke: node starts, mines, `/balance` shows a coin.
- [ ] Consensus gates: `test_consensus_vectors` 12/12, `test_mmr`, `test_winternitz`
      5/5, `test_mempool_index` 20/20, `test_sql_batch` 9/9.
- [ ] **P2P**: connects to mainnet peers, receives GREETING + IBD, syncs to tip
      (proves NIO/message transport intact after removing Maxima types).
- [ ] Two-node sync + live relay latency (proves TXPOW/TXPOWREQ path intact).
- [ ] RPC: command set works; read-only enforcement still applies.
- [ ] Wallet: key/mnemonic generation uses CSPRNG; addresses derive correctly.
- [ ] No orphaned references: `grep -riE "maxima|\bmds\b" org/` returns only the two
      cosmetic residuals above.

## 7. Conclusion

For a consensus-only node the Maxima/MDS removal is **safe and largely done**: the
layers only shared the NIO transport, P2P manager, DB and mining loops, all of which
are retained. The audit's `MiniData` 512 MB vs 16 MB divergence is a **vestige of
these removed layers**, not a live protocol requirement — so the fix is to keep the
node's small cap and (if Maxima/MDS ever return) give *them* their own limits in a
separate service, never re-inflate the consensus `MiniData`.
