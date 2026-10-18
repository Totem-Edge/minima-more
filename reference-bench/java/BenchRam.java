/*
 * BenchRam.java - RAM-scaling harness for the Java reference node
 * (spartacusrex-minima/minima-core). Measures JVM heap-in-use vs number of MMR
 * entries (the main persistent-growth structure), plus build time. Process peak
 * RSS is captured externally by run_ram_bench.sh.
 *
 * Uses MemoryMXBean heap-used after several GCs (more reliable than
 * Runtime.totalMemory()-freeMemory()). Performance reference only - NOT a
 * byte-parity oracle.
 */
import org.minima.objects.base.MiniData;
import org.minima.objects.base.MiniNumber;
import org.minima.objects.mmr.MMR;
import org.minima.objects.mmr.MMRData;

import java.lang.management.ManagementFactory;
import java.lang.management.MemoryMXBean;

public class BenchRam {
    static long usedBytes() {
        MemoryMXBean m = ManagementFactory.getMemoryMXBean();
        return m.getHeapMemoryUsage().getUsed();
    }

    static long gcUsedBytes() {
        for (int i = 0; i < 4; i++) {
            System.gc();
            try { Thread.sleep(50); } catch (InterruptedException e) { /* ignore */ }
        }
        return usedBytes();
    }

    public static void main(String[] args) {
        int mmrN = args.length > 0 ? Integer.parseInt(args[0]) : 100000;

        MMR mmr = new MMR(); // kept alive for the whole run
        long base = gcUsedBytes();
        System.out.println("heap_baseline_kb\t" + (base / 1024) + "\tkB");

        long t0 = System.nanoTime();
        for (int i = 0; i < mmrN; i++) {
            byte[] h = new byte[32];
            h[0] = (byte) i; h[1] = (byte) (i >> 8); h[2] = (byte) (i >> 16); h[3] = (byte) (i >> 24);
            mmr.addEntry(new MMRData(new MiniData(h), MiniNumber.ZERO));
        }
        long t1 = System.nanoTime();

        long after = gcUsedBytes();
        System.out.println("mmr_total_entries\t" + mmr.getTotalEntries() + "\t");
        System.out.println("heap_after_mmr_kb\t" + (after / 1024) + "\tkB");
        System.out.println("heap_delta_kb\t" + ((after - base) / 1024) + "\tkB");
        System.out.println("mmr_bytes_per_entry\t" + ((double) (after - base) / mmrN) + "\tB");
        System.out.println("mmr_build_ms\t" + ((t1 - t0) / 1e6) + "\tms");
        System.out.println("has_root\t" + (mmr.getRoot() != null) + "\t");
    }
}
