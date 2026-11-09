import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.InputStreamReader;
import java.math.BigDecimal;
import java.math.BigInteger;
import java.math.MathContext;
import java.util.ArrayList;

import org.minima.kissvm.Contract;
import org.minima.objects.Address;
import org.minima.objects.Coin;
import org.minima.objects.CoinProof;
import org.minima.objects.TxBlock;
import org.minima.objects.TxBody;
import org.minima.objects.TxHeader;
import org.minima.objects.TxPoW;
import org.minima.objects.Transaction;
import org.minima.objects.Witness;
import org.minima.objects.StateVariable;
import org.minima.objects.keys.Signature;
import org.minima.objects.keys.SignatureProof;
import org.minima.objects.keys.Winternitz;
import org.minima.system.brains.TxPoWChecker;
import org.minima.objects.base.MiniData;
import org.minima.objects.base.MiniNumber;
import org.minima.objects.base.MiniString;
import org.minima.objects.mmr.MMR;
import org.minima.objects.mmr.MMREntry;
import org.minima.objects.mmr.MMREntryNumber;
import org.minima.objects.mmr.MMRData;
import org.minima.objects.mmr.MMRProof;
import org.minima.utils.Crypto;
import org.minima.utils.Streamable;

/**
 * Level A kernel oracle — Java side.
 *
 * Prints canonical key/value lines for byte-level and hash-level operations that
 * must match the C++ node exactly. Run against the pinned minima-core checkout.
 */
public class KernelOracle {

    private static final char[] HEX = "0123456789abcdef".toCharArray();

    static String hex(byte[] bytes) {
        StringBuilder sb = new StringBuilder(bytes.length * 2);
        for (byte b : bytes) {
            sb.append(HEX[(b >> 4) & 0xF]);
            sb.append(HEX[b & 0xF]);
        }
        return sb.toString();
    }

    static String streamHex(Streamable s) throws Exception {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        DataOutputStream out = new DataOutputStream(baos);
        s.writeDataStream(out);
        out.flush();
        return hex(baos.toByteArray());
    }

    static String repeatHex(String pair, int times) {
        StringBuilder sb = new StringBuilder(pair.length() * times);
        for (int i = 0; i < times; i++) sb.append(pair);
        return sb.toString();
    }

    static byte[] hexToBytes(String hex) {
        String body = hex.startsWith("0x") ? hex.substring(2) : hex;
        if ((body.length() & 1) != 0) throw new IllegalArgumentException("odd hex");
        byte[] out = new byte[body.length() / 2];
        for (int i = 0; i < out.length; i++) {
            out[i] = (byte) Integer.parseInt(body.substring(2 * i, 2 * i + 2), 16);
        }
        return out;
    }

    static void runCorpus() throws Exception {
        BufferedReader in = new BufferedReader(new InputStreamReader(System.in));
        String line;
        while ((line = in.readLine()) != null) {
            String hex = line.trim();
            String txp;
            String txb;
            try {
                byte[] bytes = hexToBytes(hex);
                try {
                    TxPoW.ReadFromStream(new DataInputStream(new ByteArrayInputStream(bytes)));
                    txp = "OK";
                } catch (Throwable t) {
                    txp = "ERR";
                }
                try {
                    TxBlock.ReadFromStream(new DataInputStream(new ByteArrayInputStream(bytes)));
                    txb = "OK";
                } catch (Throwable t) {
                    txb = "ERR";
                }
            } catch (Throwable t) {
                txp = "ERR";
                txb = "ERR";
            }
            System.out.println(hex + "\t" + txp + "\t" + txb);
        }
    }

    static TxPoW simpleTxPoW(String txnDifficulty) {
        TxPoW t = new TxPoW();
        TxHeader h = t.getTxHeader();
        h.mBlockNumber = new MiniNumber(1);
        h.mTimeMilli = new MiniNumber(1700000000000L);
        h.mBlockDifficulty = new MiniData("0x" + repeatHex("00", 31) + "ff");
        TxBody b = t.getTxBody();
        b.mPRNG = new MiniData("0x" + repeatHex("cd", 32));
        b.mTxnDifficulty = new MiniData(txnDifficulty);
        b.mTransaction = new Transaction();
        b.mWitness = new Witness();
        return t;
    }

    static String simpleCheck(MMR tip, TxPoW t) {
        try {
            return TxPoWChecker.checkTxPoWSimple(tip, t, t, false) ? "1" : "0";
        } catch (Throwable e) {
            return "ERR";
        }
    }

    static void runValidate() {
        String low = "0x" + repeatHex("00", 31) + "01";
        String max = "0x" + repeatHex("ff", 32);
        MMR tip = new MMR();
        tip.setBlockTime(new MiniNumber(0));

        { String r = simpleCheck(tip, simpleTxPoW(low)); System.out.println("validate/A\t" + r); }
        { String r = simpleCheck(tip, simpleTxPoW(max)); System.out.println("validate/B\t" + r); }

        TxPoW c = simpleTxPoW(low);
        Coin coin = new Coin(Address.TRUE_ADDRESS.getAddressData(),
                             new MiniNumber(500), new MiniData("0x00"), true);
        c.getTxBody().mTransaction.addInput(coin);
        { String r = simpleCheck(tip, c); System.out.println("validate/C\t" + r); }

        TxPoW d = simpleTxPoW(low);
        Coin coin2 = new Coin(Address.TRUE_ADDRESS.getAddressData(),
                              new MiniNumber(500), new MiniData("0x00"), true);
        MMR other = new MMR();
        other.setBlockTime(new MiniNumber(0));
        MMRData leaf = MMRData.CreateMMRDataLeafNode(coin2, coin2.getAmount());
        MMREntry entry = other.addEntry(leaf);
        other.finalizeSet();
        coin2.setMMREntryNumber(entry.getEntryNumber());
        MMRProof proof = other.getProof(entry.getEntryNumber());
        d.getTxBody().mTransaction.addInput(coin2);
        d.getTxBody().mWitness.addCoinProof(new CoinProof(coin2, proof));
        { String r = simpleCheck(tip, d); System.out.println("validate/D\t" + r); }
    }

    public static void main(String[] args) throws Exception {
        if (args.length > 0 && args[0].equals("--validate")) {
            runValidate();
            return;
        }
        if (args.length > 0 && args[0].equals("--corpus")) {
            runCorpus();
            return;
        }
        String[] numbers = {
            "0", "1", "-1", "255", "256",
            "18446744073709551615", "1.5", "-2.5",
            "1E-44", "1E-45",
            "0.00000000000000000000000000000000000000000001",
            "123456789012345678901234567890"
        };
        for (String n : numbers) {
            String key = "mininumber/" + n;
            try {
                System.out.println(key + "\t" + streamHex(new MiniNumber(n)));
            } catch (Exception e) {
                System.out.println(key + "\tERROR");
            }
        }

        String[] datas = {
            "0x", "0x00", "0xDEADBEEF",
            "0x" + repeatHex("ab", 32)
        };
        for (String d : datas) {
            String key = "minidata/" + d;
            try {
                System.out.println(key + "\t" + streamHex(new MiniData(d)));
            } catch (Exception e) {
                System.out.println(key + "\tERROR");
            }
        }

        String[] strings = {"RETURN TRUE", "{\"name\":\"mynft\"}"};
        for (String s : strings) {
            String key = "ministring/" + s;
            try {
                System.out.println(key + "\t" + streamHex(new MiniString(s)));
            } catch (Exception e) {
                System.out.println(key + "\tERROR");
            }
        }

        MiniData base = new MiniData("0x5350415254414355534C4F5645534D494E494D41");
        for (int i = 0; i < 2; i++) {
            MiniData id = Crypto.getInstance().hashObjects(base, new MiniNumber(i));
            System.out.println("coinid/" + i + "\t" + id.to0xString());
        }

        TxHeader header = new TxHeader();
        header.mNonce = new MiniNumber(1234567);
        header.mChainID = TxHeader.MAIN_NET;
        header.mTimeMilli = new MiniNumber(1700000000000L);
        header.mBlockNumber = new MiniNumber(42);
        header.mBlockDifficulty = new MiniData("0x" + repeatHex("00", 31) + "ff");
        header.mMMRRoot = new MiniData("0x00");
        header.mMMRTotal = new MiniNumber(0);
        header.mCustomHash = MiniData.ZERO_TXPOWID;
        header.mTxBodyHash = new MiniData("0x" + repeatHex("ab", 32));

        String headerHex = streamHex(header);
        System.out.println("txheader\t" + headerHex);
        System.out.println("txpowid\t" + Crypto.getInstance().hashObject(header).to0xString());

        MiniData powId = Crypto.getInstance().hashObject(header);
        MiniData[] difficulties = {
            Crypto.MAX_HASH,
            powId,
            new MiniData("0x" + repeatHex("00", 31) + "01")
        };
        for (int i = 0; i < difficulties.length; i++) {
            System.out.println("pow/" + i + "\t" +
                (powId.isLess(difficulties[i]) ? 1 : 0));
        }

        BigDecimal difficulty =
                new BigDecimal(new BigInteger(1, header.mBlockDifficulty.getBytes()));
        BigDecimal weight = Crypto.MAX_VALDEC
                .divide(difficulty, MathContext.DECIMAL32)
                .stripTrailingZeros();
        System.out.println("weight\t" + weight.toPlainString());

        TxBody body = new TxBody();
        body.mPRNG = new MiniData("0x" + repeatHex("cd", 32));
        body.mTxnDifficulty = new MiniData("0x" + repeatHex("11", 32));
        body.mTransaction = new Transaction();
        body.mWitness = new Witness();
        body.mBurnTransaction = new Transaction();
        body.mBurnWitness = new Witness();
        body.mTxPowIDList = new ArrayList<>();
        System.out.println("txbody\t" + streamHex(body));
        System.out.println("txbodyhash\t" + Crypto.getInstance().hashObject(body).to0xString());

        TxPoW txpow = new TxPoW();
        TxHeader ph = txpow.getTxHeader();
        ph.mNonce = new MiniNumber(1234567);
        ph.mChainID = TxHeader.MAIN_NET;
        ph.mTimeMilli = new MiniNumber(1700000000000L);
        ph.mBlockNumber = new MiniNumber(42);
        ph.mBlockDifficulty = new MiniData("0x" + repeatHex("00", 31) + "ff");
        ph.mMMRRoot = new MiniData("0x00");
        ph.mMMRTotal = new MiniNumber(0);
        ph.mCustomHash = MiniData.ZERO_TXPOWID;
        ph.mTxBodyHash = new MiniData("0x" + repeatHex("ab", 32));
        TxBody pb = txpow.getTxBody();
        pb.mPRNG = new MiniData("0x" + repeatHex("cd", 32));
        pb.mTxnDifficulty = new MiniData("0x" + repeatHex("11", 32));
        System.out.println("txpow\t" + streamHex(txpow));
        System.out.println("txpowid\t" + Crypto.getInstance().hashObject(ph).to0xString());
        System.out.println("txblock\t" + streamHex(new TxBlock(txpow)));

        MMR parentmmr = new MMR();
        for (int i = 0; i < 3; i++) {
            MMRData d = MMRData.CreateMMRDataLeafNode(
                new MiniData("0x" + repeatHex(String.format("%02x", 0x51 + i), 32)),
                new MiniNumber(i));
            parentmmr.addEntry(d);
        }
        parentmmr.finalizeSet();
        TxBlock peaksBlock = new TxBlock(parentmmr, txpow, new ArrayList<TxPoW>());
        System.out.println("txblock/peaks\t" + streamHex(peaksBlock));

        MiniData inAddress = new MiniData("0x" + repeatHex("11", 32));
        Coin inputCoin = new Coin(inAddress, new MiniNumber(100), new MiniData("0x00"), true);
        inputCoin.setMMREntryNumber(new MMREntryNumber(3));
        inputCoin.setSpent(true);
        inputCoin.setBlockCreated(new MiniNumber(5));

        Coin outputCoin = new Coin(new MiniData("0x" + repeatHex("22", 32)),
                                   new MiniNumber(90), new MiniData("0x00"), true);
        outputCoin.setMMREntryNumber(new MMREntryNumber(7));
        outputCoin.setSpent(false);
        outputCoin.setBlockCreated(new MiniNumber(6));

        System.out.println("coin/input\t" + streamHex(inputCoin));
        System.out.println("coin/output\t" + streamHex(outputCoin));

        Transaction txn = new Transaction();
        txn.addInput(inputCoin);
        txn.addOutput(outputCoin);
        txn.addStateVariable(new StateVariable(0, "0x01"));
        txn.setLinkHash(new MiniData("0x" + repeatHex("33", 32)));
        System.out.println("transaction\t" + streamHex(txn));
        txn.calculateTransactionID();
        System.out.println("txid\t" + txn.getTransactionID().to0xString());

        MMR coinmmr = new MMR();
        ArrayList<Coin> proofCoins = new ArrayList<>();
        MMREntry lastEntry = null;
        for (int i = 0; i < 4; i++) {
            Coin c = new Coin(new MiniData("0x" + repeatHex(String.format("%02x", 0x31 + i), 32)),
                              new MiniNumber(100 + i), new MiniData("0x00"), true);
            proofCoins.add(c);
            MMRData leaf = MMRData.CreateMMRDataLeafNode(c, c.getAmount());
            lastEntry = coinmmr.addEntry(leaf);
        }
        coinmmr.finalizeSet();
        MMRProof proof = coinmmr.getProof(lastEntry.getEntryNumber());
        System.out.println("mmrproof\t" + streamHex(proof));
        System.out.println("coinproof\t" + streamHex(new CoinProof(proofCoins.get(3), proof)));

        MiniData wotsSeed = new MiniData("0x" + repeatHex("42", 32));
        Winternitz wots = new Winternitz(wotsSeed);
        MiniData wotsPub = wots.getPublicKey();
        MiniData wotsMsg = new MiniData("0x" + repeatHex("5a", 32));
        MiniData wotsSig = wots.sign(wotsMsg);
        System.out.println("wots/pub\t" + wotsPub.to0xString());
        System.out.println("wots/sig\t" + wotsSig.to0xString());
        System.out.println("wots/verify\t" + (Winternitz.verify(wotsPub, wotsMsg, wotsSig) ? 1 : 0));

        MMR sigmmr = new MMR();
        MMRData pubLeaf = MMRData.CreateMMRDataLeafNode(wotsPub, MiniNumber.ZERO);
        MMREntry pubEntry = sigmmr.addEntry(pubLeaf);
        sigmmr.finalizeSet();
        MMRProof pubProof = sigmmr.getProof(pubEntry.getEntryNumber());
        SignatureProof sigProof = new SignatureProof(wotsPub, wotsSig, pubProof);
        Signature signature = new Signature();
        signature.addSignatureProof(sigProof);
        Witness sigWitness = new Witness();
        sigWitness.addSignature(signature);
        System.out.println("witness\t" + streamHex(sigWitness));

        {
            MMR spendMmr = new MMR();
            spendMmr.setBlockTime(new MiniNumber(1));
            Coin spendCoin = new Coin(Address.TRUE_ADDRESS.getAddressData(),
                                      new MiniNumber(500), new MiniData("0x00"), true);
            MMRData spendLeaf = MMRData.CreateMMRDataLeafNode(spendCoin, spendCoin.getAmount());
            MMREntry spendEntry = spendMmr.addEntry(spendLeaf);
            spendMmr.finalizeSet();
            MMREntryNumber spendEntryNumber = spendEntry.getEntryNumber();
            spendCoin.setMMREntryNumber(spendEntryNumber);
            spendCoin.setBlockCreated(new MiniNumber(1));
            spendCoin.setSpent(true);
            MMRProof spendProof = spendMmr.getProofToPeak(spendEntryNumber);
            CoinProof spendCp = new CoinProof(spendCoin, spendProof);

            Coin newCoin = new Coin(new MiniData("0x" + repeatHex("61", 32)),
                                    new MiniNumber(400), new MiniData("0x00"), true);
            Transaction spendTxn = new Transaction();
            spendTxn.addInput(spendCoin);
            spendTxn.addOutput(newCoin);
            spendTxn.calculateTransactionID();

            Witness spendWitness = new Witness();
            spendWitness.addCoinProof(spendCp);

            TxPoW spendTxPow = new TxPoW();
            TxHeader spendHeader = spendTxPow.getTxHeader();
            spendHeader.mNonce = new MiniNumber(7);
            spendHeader.mTimeMilli = new MiniNumber(1700000001000L);
            spendHeader.mBlockNumber = new MiniNumber(2);
            spendHeader.mBlockDifficulty = new MiniData("0x" + repeatHex("00", 31) + "ff");
            spendHeader.mMMRRoot = spendMmr.getRoot().getData();
            spendHeader.mMMRTotal = new MiniNumber(500);
            TxBody spendBody = spendTxPow.getTxBody();
            spendBody.mPRNG = new MiniData("0x" + repeatHex("cd", 32));
            spendBody.mTxnDifficulty = new MiniData("0x" + repeatHex("11", 32));
            spendBody.mTransaction = spendTxn;
            spendBody.mWitness = spendWitness;
            spendHeader.mTxBodyHash = Crypto.getInstance().hashObject(spendBody);

            System.out.println("spend/coinproof\t" + streamHex(spendCp));
            System.out.println("spend/txpow\t" + streamHex(spendTxPow));
            TxBlock spendBlock = new TxBlock(spendMmr, spendTxPow, new ArrayList<TxPoW>());
            System.out.println("spend/txblock\t" + streamHex(spendBlock));
        }

        String[] scripts = {
            "RETURN TRUE", "RETURN FALSE", "RETURN 1 EQ 1", "RETURN 2 GT 1",
            "IF 1 EQ 1 THEN RETURN TRUE ELSE RETURN FALSE ENDIF",
            "RETURN 0x01 EQ 0x01"
        };
        for (String script : scripts) {
            Transaction tx = new Transaction();
            Witness w = new Witness();
            Contract c = new Contract(script, "", w, tx, new ArrayList<StateVariable>());
            c.run();
            System.out.println("script/" + script + "\t" +
                (c.isParseOK() ? 1 : 0) + ";" + (c.isSuccess() ? 1 : 0) + ";" +
                (c.isException() ? 1 : 0));
        }

        MMR mmr = new MMR();
        for (int i = 0; i < 4; i++) {
            String leaf = "0x" + repeatHex(String.format("%02x", i + 1), 32);
            MMRData data = MMRData.CreateMMRDataLeafNode(new MiniData(leaf), new MiniNumber(i));
            mmr.addEntry(data);
        }
        mmr.finalizeSet();
        System.out.println("mmrroot\t" + mmr.getRoot().getData().to0xString());
    }
}
