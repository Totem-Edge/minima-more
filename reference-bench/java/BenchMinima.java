/*
 * BenchMinima.java
 *
 * Performance harness for the Java reference node (spartacusrex-minima/minima-core).
 *
 * IMPORTANT: minima-core is a PERFORMANCE reference only. It must NOT be used for
 * byte-parity / consensus correctness checks (its address-derivation path may
 * differ from this C++ node). Correctness is anchored to this repo's own
 * test_consensus_vectors and the Totem Java interop KATs.
 *
 * It benchmarks the same primitives this node optimised:
 *   - Winternitz (WOTS) public-key derivation
 *   - Winternitz sign
 *   - Winternitz verify
 *   - SHA3-256 over a 224-byte header (the mining inner loop)
 *   - MMR addEntry + getRoot, and proof/verify
 *
 * Output is machine-readable: "metric<TAB>value<TAB>unit".
 *
 * Build/run: see reference-bench/build_minima_core.sh and run_java_bench.sh
 */

import org.minima.objects.base.MiniData;
import org.minima.objects.base.MiniNumber;
import org.minima.objects.keys.Winternitz;
import org.minima.objects.mmr.MMR;
import org.minima.objects.mmr.MMRData;
import org.minima.objects.mmr.MMREntry;
import org.minima.objects.mmr.MMREntryNumber;
import org.minima.objects.mmr.MMRProof;
import org.minima.utils.Crypto;

public class BenchMinima {

    static long now() { return System.nanoTime(); }
    static double ms(long a, long b) { return (b - a) / 1e6; }

    static void metric(String name, double value, String unit) {
        System.out.printf("%s\t%.6f\t%s%n", name, value, unit);
    }

    // best-of-N reps, each rep measures `iters` operations
    interface Op { void run(int iters); }

    static double bestMsPerOp(int reps, int iters, Op op) {
        // warmup
        op.run(Math.max(1, iters / 10));
        double best = Double.MAX_VALUE;
        for (int r = 0; r < reps; r++) {
            long t0 = now();
            op.run(iters);
            long t1 = now();
            best = Math.min(best, ms(t0, t1) / iters);
        }
        return best;
    }

    public static void main(String[] args) {
        int wotsIters = args.length > 0 ? Integer.parseInt(args[0]) : 200;
        int mmrN      = args.length > 1 ? Integer.parseInt(args[1]) : 20000;
        int hashIters = args.length > 2 ? Integer.parseInt(args[2]) : 200000;
        int reps      = 3;

        // ---- environment ----
        Runtime rt = Runtime.getRuntime();
        System.out.println("# minima-core (spartacusrex) performance reference - NOT a byte-parity oracle");
        System.out.println("java.version\t" + System.getProperty("java.version") + "\t");
        System.out.println("os.arch\t" + System.getProperty("os.arch") + "\t");
        System.out.println("availableProcessors\t" + rt.availableProcessors() + "\t");
        System.out.println("maxHeapMB\t" + (rt.maxMemory() / (1024 * 1024)) + "\t");
        System.out.println("wotsIters\t" + wotsIters + "\t");
        System.out.println("mmrN\t" + mmrN + "\t");
        System.out.println("hashIters\t" + hashIters + "\t");

        // ---- WOTS ----
        final MiniData seed = new MiniData(
            "0x0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20");
        final MiniData msg = new MiniData(new byte[32]);

        try {
            double pubMs = bestMsPerOp(reps, wotsIters, (n) -> {
                MiniData p = null;
                for (int i = 0; i < n; i++) p = new Winternitz(seed).getPublicKey();
                if (p == null) throw new IllegalStateException();
            });
            metric("wots_pubkey_ms", pubMs, "ms/op");
        } catch (Throwable t) { System.out.println("wots_pubkey_ms\tERROR\t" + t); }

        final Winternitz w = new Winternitz(seed);
        final MiniData pub = w.getPublicKey();
        final MiniData sig = w.sign(msg);

        try {
            double signMs = bestMsPerOp(reps, wotsIters, (n) -> {
                MiniData s = null;
                for (int i = 0; i < n; i++) s = w.sign(msg);
                if (s == null) throw new IllegalStateException();
            });
            metric("wots_sign_ms", signMs, "ms/op");
        } catch (Throwable t) { System.out.println("wots_sign_ms\tERROR\t" + t); }

        try {
            double verMs = bestMsPerOp(reps, wotsIters, (n) -> {
                for (int i = 0; i < n; i++) Winternitz.verify(pub, msg, sig);
            });
            metric("wots_verify_ms", verMs, "ms/op");
        } catch (Throwable t) { System.out.println("wots_verify_ms\tERROR\t" + t); }

        // ---- SHA3-256 over a 224-byte header (mining inner loop) ----
        try {
            final byte[] header = new byte[224];
            double usPerHash = bestMsPerOp(reps, hashIters, (n) -> {
                byte[] out = null;
                for (int i = 0; i < n; i++) out = Crypto.getInstance().hashData(header);
                if (out == null) throw new IllegalStateException();
            });
            metric("sha3_224_us_per_hash", usPerHash * 1000.0, "us/hash");
            metric("sha3_224_hashes_per_sec", 1e6 / (usPerHash * 1000.0), "h/s");
        } catch (Throwable t) { System.out.println("sha3_224_hashes_per_sec\tERROR\t" + t); }

        // ---- MMR ----
        try {
            MMR mmr = new MMR();
            long t0 = now();
            MMREntryNumber last = null;
            for (int i = 0; i < mmrN; i++) {
                byte[] h = new byte[32];
                h[0] = (byte)(i & 0xFF);
                h[1] = (byte)((i >> 8) & 0xFF);
                MMREntry e = mmr.addEntry(new MMRData(new MiniData(h), MiniNumber.ZERO));
                last = e.getEntryNumber();
            }
            MMRData root = mmr.getRoot();
            long t1 = now();
            metric("mmr_add_entries_ms", ms(t0, t1), "ms");
            metric("mmr_add_entries_per_sec", mmrN / (ms(t0, t1) / 1000.0), "entries/s");
            if (root == null) throw new IllegalStateException("null root");

            // proof + check for the last entry
            long t2 = now();
            MMRProof proof = mmr.getProof(last);
            boolean ok = mmr.checkProof(new MMRData(new MiniData(new byte[32]), MiniNumber.ZERO), proof);
            long t3 = now();
            metric("mmr_getProof_check_us", (ms(t2, t3) * 1000.0), "us");
            System.out.println("mmr_proof_check_ok\t" + ok + "\t(informational; value differs)");
        } catch (Throwable t) { System.out.println("mmr_add_entries_per_sec\tERROR\t" + t); }

        System.out.println("# done");
    }
}
