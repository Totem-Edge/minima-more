/*
 * BenchMempool.java - Mempool (RamDB) RAM-scaling harness for the Java reference
 * node (spartacusrex-minima/minima-core). Mirrors reference-bench/cpp/bench_mempool.cpp:
 * builds `numTxns` transaction TxPoWs (each with `coinsPerTx` input coins) into a
 * RamDB, and reports JVM heap-in-use vs transactions/coins, plus build time and
 * checkForCoinID lookup time (the Java reference scans the pool linearly here).
 *
 * Uses MemoryMXBean heap-used after several GCs (more reliable than
 * Runtime.totalMemory()-freeMemory()). Performance reference only - NOT a
 * byte-parity oracle.
 *
 * Usage: BenchMempool [numTxns=100000] [coinsPerTx=2] [lookups=1000]
 */
import org.minima.database.txpowdb.ram.RamDB;
import org.minima.objects.Coin;
import org.minima.objects.CoinProof;
import org.minima.objects.Token;
import org.minima.objects.Transaction;
import org.minima.objects.TxPoW;
import org.minima.objects.Witness;
import org.minima.objects.base.MiniData;
import org.minima.objects.base.MiniNumber;
import org.minima.objects.mmr.MMRProof;

import java.lang.management.ManagementFactory;
import java.lang.management.MemoryMXBean;
import java.util.ArrayList;

public class BenchMempool {
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

    // A deterministic, non-zero 32-byte coin id unique to (txIndex, coinIndex).
    static MiniData coinId(int txIndex, int coinIndex) {
        byte[] b = new byte[32];
        b[0] = (byte) 0xAB;
        b[4] = (byte) txIndex;
        b[5] = (byte) (txIndex >> 8);
        b[6] = (byte) (txIndex >> 16);
        b[7] = (byte) (txIndex >> 24);
        b[8] = (byte) coinIndex;
        b[9] = (byte) (coinIndex >> 8);
        return new MiniData(b);
    }

    // Build a tiny transaction TxPoW whose witness carries the given input coins.
    // setHeaderBodyHash() is required so different transactions hash to different
    // TxPoW IDs (calculateTXPOWID hashes the header, which embeds the body hash).
    static TxPoW makeTxPoW(ArrayList<Coin> coins) {
        TxPoW txp = new TxPoW();
        Transaction t = new Transaction();
        t.addInput(coins.get(0));
        txp.setTransaction(t);
        Witness w = new Witness();
        for (Coin c : coins) {
            w.addCoinProof(new CoinProof(c, new MMRProof()));
        }
        txp.setWitness(w);
        txp.setHeaderBodyHash();
        txp.calculateTXPOWID();
        return txp;
    }

    public static void main(String[] args) {
        int numTxns   = args.length > 0 ? Integer.parseInt(args[0]) : 100000;
        int coinsPerTx = args.length > 1 ? Integer.parseInt(args[1]) : 2;
        int lookups   = args.length > 2 ? Integer.parseInt(args[2]) : 1000;

        MiniData addr = new MiniData("0x0000000000000000000000000000000000000000000000000000000000000000");

        RamDB ram = new RamDB();                 // kept alive for the whole run
        ArrayList<MiniData> present = new ArrayList<>();

        long base = gcUsedBytes();
        System.out.println("heap_baseline_kb\t" + (base / 1024) + "\tkB");

        long t0 = System.nanoTime();
        for (int i = 0; i < numTxns; i++) {
            ArrayList<Coin> coins = new ArrayList<>(coinsPerTx);
            for (int j = 0; j < coinsPerTx; j++) {
                coins.add(new Coin(coinId(i, j), addr, new MiniNumber(1 + i), Token.TOKENID_MINIMA));
            }
            TxPoW txp = makeTxPoW(coins);
            ram.addTxPoW(txp);
            if (present.size() < lookups) {
                present.add(coins.get(0).getCoinID());
            }
        }
        long t1 = System.nanoTime();

        long after = gcUsedBytes();
        long delta = after - base;
        double coins = (double) numTxns * coinsPerTx;

        System.out.println("mempool_txns\t" + ram.getSize() + "\t");
        System.out.println("mempool_coins\t" + (long) coins + "\t");
        System.out.println("mempool_build_ms\t" + ((t1 - t0) / 1e6) + "\tms");
        System.out.println("heap_after_mempool_kb\t" + (after / 1024) + "\tkB");
        System.out.println("mempool_delta_kb\t" + (delta / 1024) + "\tkB");
        System.out.println("mempool_bytes_per_txpow\t" + ((double) delta / numTxns) + "\tB");
        System.out.println("mempool_bytes_per_coin\t" + ((double) delta / coins) + "\tB");

        // Lookup timing: "is this coin in the mempool?" (double-spend check).
        long hits = 0;
        long tl0 = System.nanoTime();
        for (MiniData id : present) {
            if (ram.checkForCoinID(id)) hits++;
        }
        long tl1 = System.nanoTime();
        double lookupMs = (tl1 - tl0) / 1e6;
        System.out.println("mempool_lookups\t" + present.size() + "\t");
        System.out.println("mempool_lookup_hits\t" + hits + "\t");
        System.out.println("mempool_lookup_ms\t" + lookupMs + "\tms");
        if (!present.isEmpty()) {
            System.out.println("mempool_lookup_us_per\t" + ((lookupMs * 1000.0) / present.size()) + "\tus");
        }
    }
}
