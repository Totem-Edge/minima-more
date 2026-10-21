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
| Peak RSS | **1.01 GB** (`VmHWM`; log 914 MB) | **3.16 GB** |
| Archive DB | **2.45 GB** | **13 GB** |
| Total data dir | ~2.9 GB (archive + 473 MB on-chain) | ~13 GB |
| Blocks imported | **2,376,119** (after tail fill; 2,374,071 before) | 2,374,912 |
| Gaps / missing chunks | 0 / 0 | n/a (H2) |
| Shutdown | segfault/hang after `All saved` (exit 139) | clean |

C++ `importraw` is ~2.8x slower but uses ~3.1x less RSS and ~4.6x less disk
(5.3x smaller archive DB).

## Tail completeness fix

The initial C++ import stopped at 2,371,584: `importraw` never posted the empty
end-of-stream IBD, so the final recalculation (and its archive flush) never ran.
Posting the empty IBD + draining the processor raised it to 2,374,071.

The remaining **2,048 blocks** were by design: `recalculateTree()` only calls
`arch->saveBlock()` for blocks entering the **cascade**, and the active tree
retains the last `MINIMA_CASCADE_START_DEPTH = 2048` blocks. A post-import tail
pass re-reads the raw stream and saves only blocks above the archive tip:

```
before=2374071 added=2048 after=2376119 last=2376119
db count=2376119 min=1 max=2376119 gaps=0 missing_chunks=0
lookups=260 ok=260 fail=0 avg_us=1793.5
```

C++ now imports the **full header count 2,376,119**; Java's archive stops at
2,374,912 (1,207 short). Tail fill costs one extra raw pass (~1–2 min).

Peer backfill on the earlier C++ DB reached 2,375,845; the record battery there
passed 260/260 at 1.92 ms, 242 MB idle with peers (HWM 440 MB).

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
