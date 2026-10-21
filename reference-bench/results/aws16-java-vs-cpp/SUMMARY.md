# Java vs C++ archive import + IBD cooldown (16 GB box)

Same host, same raw file, same `importraw` path. Host: AWS `c6g.2xlarge` —
8 vCPU, **16 GB RAM**, **50 GB gp3**, aarch64, Ubuntu 24.04.

- C++: latest `main` (`c2e57b5` + IBD-limit fix), `build-mcp/minima`.
- Java: `spartacusrex-minima/minima-core` @ `c2a54a2`, OpenJDK 17, `-Xmx12g`.
- Input: `/tmp/archive.raw.dat` (1.5 GB, live `archive.raw.dat`); header says
  **2,376,119** blocks.

## Import (synchronous `archive action:importraw`)

| Metric | C++ (chunked SQLite) | Java (H2) |
|---|---:|---:|
| Wall time | **78.9 min** | **28.0 min** |
| Throughput | **501 blocks/s** | **1,413 blocks/s** |
| Peak RSS | **994 MB** (`VmHWM`) | **3.16 GB** |
| Archive DB | **2.45 GB** | **13 GB** |
| Total data dir | ~2.9 GB (archive + 473 MB on-chain) | ~13 GB |
| Blocks imported | 2,371,584 | 2,374,912 |
| Gaps / missing chunks | 0 / 0 | n/a (H2) |
| Shutdown | **hangs busy-spin** after `All saved` (killed) | clean |

C++ `importraw` is ~2.8x slower but uses ~3.2x less RSS and ~4.4x less disk
(5.3x smaller archive DB).

## C++ tail-loss bug (found by the comparison)

Both counts land on 256-block boundaries, but C++ imports **3,328 fewer blocks**
than Java from the same file (13 IBD batches). The raw header is 2,376,119;
Java reaches 2,374,912 (1,207 short), C++ reaches 2,371,584 (4,535 short).
`archive action:import` (async resync) showed the same count. This is a
`RawArchiveInput`/raw-IBD tail issue in C++, not a chunk-SQLite issue — the
imported prefix passes integrity and parent checks.

Peer backfill on the C++ node then took the DB to **2,375,845** blocks, where the
record battery passed: 260/260 lookups, 0 gaps, 0 missing chunks, **1.92 ms**
average lookup, 242 MB RSS idle with peers (HWM 440 MB).

## IBD cooldown fix

`NIOMessage` logged “Already sent an IBD … in last 30 mins” but stored addresses
in a `HashSet` that was **never expired** — only cleared on startup/network
restart. One IBD per peer per process lifetime.

Now:
- `mHaveSentIBDRecently` is `peer -> steady_clock::time_point`;
- `-ibdlimit <minutes>` (default **30**, **0 = unlimited**) — actual expiry;
- the log prints the configured window;
- thread-safe under the existing mutex.

Verified with `-ibdlimit 0`: the same peer (`173.212.202.190:9011`) re-greeted
three times with **no** “Already sent” log, and the archive backfilled
2,371,584 → 2,375,845 (+4,261) from peers.

For fast first sync from trusted peers use `-ibdlimit 0` (or a small value);
keep the default for public nodes, since an IBD is several MB and is a DoS lever.
Java has the same latent behaviour (boolean, no expiry); C++ is now stricter and
configurable.

## Raw numbers

```
C++:  All blocks processed..  78.9 min, VmHWM 1,017,520 kB, count 2,371,584
Java: All blocks processed..  28:00.73,  Maximum resident 3,156,676 kB,
      archive.mv.db 13 GB, count 2,374,912
```
