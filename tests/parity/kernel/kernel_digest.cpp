// Level A kernel digest — C++ side.
//
// Must print exactly the same key/value lines as KernelOracle.java (the Java
// oracle at the pinned minima-core commit). See run_kernel.sh.
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/objects/address.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/keys/signature.hpp"
#include "org/minima/objects/keys/signature_proof.hpp"
#include "org/minima/objects/keys/winternitz.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_entry.hpp"
#include "org/minima/objects/mmr/m_m_r_entry_number.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"
#include "org/minima/objects/state_variable.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_body.hpp"
#include "org/minima/objects/tx_header.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/system/brains/tx_po_w_checker.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/utils/crypto.hpp"

using org::minima::kissvm::Contract;
using org::minima::objects::Coin;
using org::minima::objects::CoinProof;
using org::minima::objects::TxBlock;
using org::minima::objects::TxBody;
using org::minima::objects::TxHeader;
using org::minima::objects::TxPoW;
using org::minima::objects::Transaction;
using org::minima::objects::Witness;
using org::minima::objects::StateVariable;
using org::minima::objects::keys::Signature;
using org::minima::objects::keys::SignatureProof;
using org::minima::objects::keys::Winternitz;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniString;
using org::minima::objects::mmr::MMR;
using org::minima::objects::mmr::MMRData;
using org::minima::objects::mmr::MMREntry;
using org::minima::objects::mmr::MMREntryNumber;
using org::minima::objects::mmr::MMRProof;
using org::minima::utils::Crypto;

namespace {

std::string hex(const std::string& bytes) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (unsigned char c : bytes) {
        out.push_back(digits[c >> 4]);
        out.push_back(digits[c & 0x0F]);
    }
    return out;
}

template <typename T>
std::string streamHex(T& object) {
    std::ostringstream out(std::ios::out | std::ios::binary);
    object.writeDataStream(out);
    return hex(out.str());
}

std::string hexToBytes(const std::string& hex) {
    const std::string h = hex.rfind("0x", 0) == 0 ? hex.substr(2) : hex;
    if (h.size() % 2 != 0) throw std::invalid_argument("odd hex");
    std::string out;
    out.reserve(h.size() / 2);
    for (std::size_t i = 0; i < h.size(); i += 2) {
        out.push_back(static_cast<char>(std::stoi(h.substr(i, 2), nullptr, 16)));
    }
    return out;
}

std::string repeat(const std::string& pair, int times) {
    std::string out;
    out.reserve(pair.size() * static_cast<size_t>(times));
    for (int i = 0; i < times; ++i) out += pair;
    return out;
}

} // namespace

static TxPoW simpleTxPoW(const std::string& txnDifficulty) {
    TxPoW t;
    auto& h = t.getTxHeader();
    h.mBlockNumber = MiniNumber(1);
    h.mTimeMilli = MiniNumber(1700000000000LL);
    h.mBlockDifficulty = MiniData("0x" + std::string(62, '0') + "ff");
    auto* b = t.getTxBody();
    b->mPRNG = std::make_unique<MiniData>("0x" + std::string(64, 'c') + "d");
    b->mTxnDifficulty = std::make_unique<MiniData>(txnDifficulty);
    b->mTransaction = std::make_unique<Transaction>();
    b->mWitness = std::make_unique<Witness>();
    return t;
}

static std::string simpleCheck(MMR& tip, TxPoW& t) {
    try {
        return org::minima::system::brains::TxPoWChecker::checkTxPoWSimple(tip, t, t, false)
                   ? "1" : "0";
    } catch (...) {
        return "ERR";
    }
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--validate") {
        const std::string low = "0x" + std::string(62, '0') + "01";
        const std::string max = "0x" + std::string(64, 'f');
        MMR tip;
        tip.setBlockTime(MiniNumber(0));

        TxPoW a = simpleTxPoW(low);
        { std::string r = simpleCheck(tip, a); std::cout << "validate/A\t" << r << "\n"; }
        TxPoW b = simpleTxPoW(max);
        { std::string r = simpleCheck(tip, b); std::cout << "validate/B\t" << r << "\n"; }

        TxPoW c = simpleTxPoW(low);
        auto coin = std::make_shared<Coin>(
            org::minima::objects::Address::getTrueAddress().getAddressData(),
            MiniNumber(500), MiniData("0x00"), true);
        c.getTxBody()->mTransaction->addInput(coin);
        { std::string r = simpleCheck(tip, c); std::cout << "validate/C\t" << r << "\n"; }

        TxPoW d = simpleTxPoW(low);
        Coin coin2(org::minima::objects::Address::getTrueAddress().getAddressData(),
                   MiniNumber(500), MiniData("0x00"), true);
        MMR other;
        other.setBlockTime(MiniNumber(0));
        auto leaf = MMRData::CreateMMRDataLeafNode(coin2, coin2.getAmount());
        auto entry = other.addEntry(*leaf);
        other.finalizeSet();
        coin2.setMMREntryNumber(entry.getEntryNumber());
        MMRProof proof = other.getProof(entry.getEntryNumber());
        auto coin2Ptr = std::make_shared<Coin>(coin2);
        d.getTxBody()->mTransaction->addInput(coin2Ptr);
        d.getTxBody()->mWitness->addCoinProof(std::make_unique<CoinProof>(
            coin2Ptr, std::make_shared<MMRProof>(proof)));
        { std::string r = simpleCheck(tip, d); std::cout << "validate/D\t" << r << "\n"; }
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "--corpus") {
        std::string line;
        while (std::getline(std::cin, line)) {
            std::string txp;
            std::string txb;
            try {
                const std::string bytes = hexToBytes(line);
                try {
                    std::istringstream in(bytes, std::ios::binary);
                    auto txpow = TxPoW::ReadFromStream(in);
                    txp = txpow ? "OK" : "ERR";
                } catch (...) {
                    txp = "ERR";
                }
                try {
                    std::istringstream in(bytes, std::ios::binary);
                    auto block = TxBlock::ReadFromStream(in);
                    txb = block ? "OK" : "ERR";
                } catch (...) {
                    txb = "ERR";
                }
            } catch (...) {
                txp = "ERR";
                txb = "ERR";
            }
            std::cout << line << "\t" << txp << "\t" << txb << "\n";
        }
        return 0;
    }

    const std::vector<std::string> numbers = {
        "0", "1", "-1", "255", "256",
        "18446744073709551615", "1.5", "-2.5",
        "1E-44", "1E-45",
        "0.00000000000000000000000000000000000000000001",
        "123456789012345678901234567890"
    };
    for (const auto& n : numbers) {
        const std::string key = "mininumber/" + n;
        try {
            MiniNumber mn(n);
            std::cout << key << "\t" << streamHex(mn) << "\n";
        } catch (const std::exception&) {
            std::cout << key << "\tERROR\n";
        }
    }

    const std::vector<std::string> datas = {
        "0x", "0x00", "0xDEADBEEF",
        "0x" + repeat("ab", 32)
    };
    for (const auto& d : datas) {
        const std::string key = "minidata/" + d;
        try {
            MiniData md(d);
            std::cout << key << "\t" << streamHex(md) << "\n";
        } catch (const std::exception&) {
            std::cout << key << "\tERROR\n";
        }
    }

    const std::vector<std::string> strings = {"RETURN TRUE", "{\"name\":\"mynft\"}"};
    for (const auto& s : strings) {
        const std::string key = "ministring/" + s;
        try {
            MiniString ms(s);
            std::cout << key << "\t" << streamHex(ms) << "\n";
        } catch (const std::exception&) {
            std::cout << key << "\tERROR\n";
        }
    }

    try {
        MiniData base("0x5350415254414355534C4F5645534D494E494D41");
        for (int i = 0; i < 2; ++i) {
            MiniNumber output(i);
            MiniData id = Crypto::getInstance().hashObjects(base, output);
            std::cout << "coinid/" << i << "\t" << id.to0xString() << "\n";
        }
    } catch (const std::exception&) {
        std::cout << "coinid/ERROR\n";
    }

    TxHeader header;
    header.mNonce = MiniNumber(1234567);
    header.mChainID = TxHeader::MAIN_NET;
    header.mTimeMilli = MiniNumber(1700000000000LL);
    header.mBlockNumber = MiniNumber(42);
    header.mBlockDifficulty = MiniData("0x" + repeat("00", 31) + "ff");
    header.mMMRRoot = MiniData("0x00");
    header.mMMRTotal = MiniNumber(0);
    header.mCustomHash = MiniData::ZERO_TXPOWID();
    header.mTxBodyHash = MiniData("0x" + repeat("ab", 32));

    std::cout << "txheader\t" << streamHex(header) << "\n";
    std::cout << "txpowid\t"
              << Crypto::getInstance().hashObject(header).to0xString() << "\n";

    {
        MiniData powId = Crypto::getInstance().hashObject(header);
        const std::vector<MiniData> difficulties = {
            Crypto::MAX_HASH(),
            powId,
            MiniData("0x" + repeat("00", 31) + "01")
        };
        for (std::size_t i = 0; i < difficulties.size(); ++i) {
            std::cout << "pow/" << i << "\t"
                      << (powId.isLess(difficulties[i]) ? 1 : 0) << "\n";
        }
    }

    {
        using boost::multiprecision::cpp_dec_float_100;
        cpp_dec_float_100 maxdec(Crypto::MAX_VALDEC());
        cpp_dec_float_100 blkdec(header.mBlockDifficulty.getDataValue());
        std::ostringstream weight;
        weight << std::setprecision(7) << (maxdec / blkdec);
        MiniNumber wn = MiniNumber::fromWideString(weight.str());
        std::cout << "weight\t" << wn.getAsBigDecimal() << "\n";
    }

    {
        TxBody body;
        body.mPRNG = std::make_unique<MiniData>("0x" + repeat("cd", 32));
        body.mTxnDifficulty = std::make_unique<MiniData>("0x" + repeat("11", 32));
        body.mTransaction = std::make_unique<Transaction>();
        body.mWitness = std::make_unique<Witness>();
        body.mBurnTransaction = std::make_unique<Transaction>();
        body.mBurnWitness = std::make_unique<Witness>();
        body.mTxPowIDList.clear();
        std::cout << "txbody\t" << streamHex(body) << "\n";
        std::cout << "txbodyhash\t"
                  << Crypto::getInstance().hashObject(body).to0xString() << "\n";

        TxPoW txpow;
        auto& ph = txpow.getTxHeader();
        ph.mNonce = MiniNumber(1234567);
        ph.mChainID = TxHeader::MAIN_NET;
        ph.mTimeMilli = MiniNumber(1700000000000LL);
        ph.mBlockNumber = MiniNumber(42);
        ph.mBlockDifficulty = MiniData("0x" + repeat("00", 31) + "ff");
        ph.mMMRRoot = MiniData("0x00");
        ph.mMMRTotal = MiniNumber(0);
        ph.mCustomHash = MiniData::ZERO_TXPOWID();
        ph.mTxBodyHash = MiniData("0x" + repeat("ab", 32));

        auto* pb = txpow.getTxBody();
        pb->mPRNG = std::make_unique<MiniData>("0x" + repeat("cd", 32));
        pb->mTxnDifficulty = std::make_unique<MiniData>("0x" + repeat("11", 32));

        std::cout << "txpow\t" << streamHex(txpow) << "\n";
        std::cout << "txpowid\t"
                  << Crypto::getInstance().hashObject(ph).to0xString() << "\n";

        TxBlock block(txpow);
        std::cout << "txblock\t" << streamHex(block) << "\n";

        MMR parentmmr;
        for (int i = 0; i < 3; ++i) {
            std::ostringstream leafHex;
            leafHex << std::hex << std::setfill('0') << std::setw(2) << (0x51 + i);
            MiniData leafData("0x" + repeat(leafHex.str(), 32));
            MiniNumber leafSum(i);
            auto leaf = MMRData::CreateMMRDataLeafNode(leafData, leafSum);
            parentmmr.addEntry(*leaf);
        }
        parentmmr.finalizeSet();
        std::vector<TxPoW*> noTransactions;
        TxBlock peaksBlock(parentmmr, txpow, noTransactions);
        std::cout << "txblock/peaks\t" << streamHex(peaksBlock) << "\n";

        auto inputCoin = std::make_shared<Coin>(
            MiniData("0x" + repeat("11", 32)), MiniNumber(100),
            MiniData("0x00"), true);
        inputCoin->setMMREntryNumber(MMREntryNumber(3));
        inputCoin->setSpent(true);
        inputCoin->setBlockCreated(MiniNumber(5));

        auto outputCoin = std::make_unique<Coin>(
            MiniData("0x" + repeat("22", 32)), MiniNumber(90),
            MiniData("0x00"), true);
        outputCoin->setMMREntryNumber(MMREntryNumber(7));
        outputCoin->setSpent(false);
        outputCoin->setBlockCreated(MiniNumber(6));

        std::cout << "coin/input\t" << streamHex(*inputCoin) << "\n";
        std::cout << "coin/output\t" << streamHex(*outputCoin) << "\n";

        Transaction txn;
        txn.addInput(inputCoin);
        txn.addOutput(std::move(outputCoin));
        txn.addStateVariable(std::make_unique<StateVariable>(0, "0x01"));
        txn.setLinkHash(MiniData("0x" + repeat("33", 32)));
        std::cout << "transaction\t" << streamHex(txn) << "\n";
        txn.calculateTransactionID();
        std::cout << "txid\t" << txn.getTransactionID().to0xString() << "\n";

        {
            MMR coinmmr;
            std::vector<std::shared_ptr<Coin>> proofCoins;
            MMREntry lastEntry;
            for (int i = 0; i < 4; ++i) {
                std::ostringstream addr;
                addr << std::hex << std::setfill('0') << std::setw(2) << (0x31 + i);
                auto c = std::make_shared<Coin>(MiniData("0x" + repeat(addr.str(), 32)),
                                                MiniNumber(100 + i), MiniData("0x00"), true);
                proofCoins.push_back(c);
                auto leaf = MMRData::CreateMMRDataLeafNode(*c, c->getAmount());
                lastEntry = coinmmr.addEntry(*leaf);
            }
            coinmmr.finalizeSet();
            MMRProof proof = coinmmr.getProof(lastEntry.getEntryNumber());
            std::cout << "mmrproof\t" << streamHex(proof) << "\n";
            auto proofPtr = std::make_shared<MMRProof>(proof);
            CoinProof coinProof(proofCoins.back(), proofPtr);
            std::cout << "coinproof\t" << streamHex(coinProof) << "\n";
        }

        {
            MiniData wotsSeed("0x" + repeat("42", 32));
            Winternitz wots(wotsSeed);
            MiniData wotsPub = wots.getPublicKey();
            MiniData wotsMsg("0x" + repeat("5a", 32));
            MiniData wotsSig = wots.sign(wotsMsg);
            std::cout << "wots/pub\t" << wotsPub.to0xString() << "\n";
            std::cout << "wots/sig\t" << wotsSig.to0xString() << "\n";
            std::cout << "wots/verify\t"
                      << (Winternitz::verify(wotsPub, wotsMsg, wotsSig) ? 1 : 0) << "\n";

            MMR sigmmr;
            auto pubLeaf = MMRData::CreateMMRDataLeafNode(wotsPub, MiniNumber::ZERO());
            auto pubEntry = sigmmr.addEntry(*pubLeaf);
            sigmmr.finalizeSet();
            MMRProof pubProof = sigmmr.getProof(pubEntry.getEntryNumber());
            auto sigProof = std::make_unique<SignatureProof>(wotsPub, wotsSig, pubProof);
            auto signature = std::make_unique<Signature>();
            signature->addSignatureProof(std::move(sigProof));
            Witness sigWitness;
            sigWitness.addSignature(std::move(signature));
            std::cout << "witness\t" << streamHex(sigWitness) << "\n";
        }

        {
            MMR spendMmr;
            spendMmr.setBlockTime(MiniNumber(1));
            Coin spendCoin(org::minima::objects::Address::getTrueAddress().getAddressData(),
                           MiniNumber(500), MiniData("0x00"), true);
            auto spendLeaf = MMRData::CreateMMRDataLeafNode(spendCoin, spendCoin.getAmount());
            auto spendEntry = spendMmr.addEntry(*spendLeaf);
            spendMmr.finalizeSet();
            MMREntryNumber spendEntryNumber = spendEntry.getEntryNumber();
            spendCoin.setMMREntryNumber(spendEntryNumber);
            spendCoin.setBlockCreated(MiniNumber(1));
            spendCoin.setSpent(true);
            MMRProof spendProof = spendMmr.getProofToPeak(spendEntryNumber);
            auto spendCoinPtr = std::make_shared<Coin>(spendCoin);
            auto spendCp = std::make_shared<CoinProof>(
                spendCoinPtr, std::make_shared<MMRProof>(spendProof));

            Coin newCoin(MiniData("0x" + repeat("61", 32)),
                         MiniNumber(400), MiniData("0x00"), true);
            Transaction spendTxn;
            spendTxn.addInput(spendCoinPtr);
            spendTxn.addOutput(std::make_unique<Coin>(newCoin));
            spendTxn.calculateTransactionID();

            auto spendWitness = std::make_unique<Witness>();
            spendWitness->addCoinProof(std::make_unique<CoinProof>(*spendCp));

            TxPoW spendTxPow;
            auto& spendHeader = spendTxPow.getTxHeader();
            spendHeader.mNonce = MiniNumber(7);
            spendHeader.mTimeMilli = MiniNumber(1700000001000LL);
            spendHeader.mBlockNumber = MiniNumber(2);
            spendHeader.mBlockDifficulty = MiniData("0x" + repeat("00", 31) + "ff");
            spendHeader.mMMRRoot = spendMmr.getRoot()->getData();
            spendHeader.mMMRTotal = MiniNumber(500);
            auto* spendBody = spendTxPow.getTxBody();
            spendBody->mPRNG = std::make_unique<MiniData>("0x" + repeat("cd", 32));
            spendBody->mTxnDifficulty = std::make_unique<MiniData>("0x" + repeat("11", 32));
            spendBody->mTransaction = std::make_unique<Transaction>(spendTxn);
            spendBody->mWitness = std::move(spendWitness);
            spendHeader.mTxBodyHash = Crypto::getInstance().hashObject(*spendBody);

            std::cout << "spend/coinproof\t" << streamHex(*spendCp) << "\n";
            std::cout << "spend/txpow\t" << streamHex(spendTxPow) << "\n";
            std::vector<TxPoW*> noTxs;
            TxBlock spendBlock(spendMmr, spendTxPow, noTxs);
            std::cout << "spend/txblock\t" << streamHex(spendBlock) << "\n";
        }

        const std::vector<std::string> scripts = {
            "RETURN TRUE", "RETURN FALSE", "RETURN 1 EQ 1", "RETURN 2 GT 1",
            "IF 1 EQ 1 THEN RETURN TRUE ELSE RETURN FALSE ENDIF",
            "RETURN 0x01 EQ 0x01"
        };
        for (const auto& script : scripts) {
            Transaction tx;
            Witness w;
            std::vector<std::unique_ptr<StateVariable>> prev;
            Contract contract(script, std::string(""), w, tx, std::move(prev));
            contract.run();
            std::cout << "script/" << script << "\t"
                      << (contract.isParseOK() ? 1 : 0) << ";"
                      << (contract.isSuccess() ? 1 : 0) << ";"
                      << (contract.isException() ? 1 : 0) << "\n";
        }
    }

    try {
        MMR mmr;
        for (int i = 0; i < 4; ++i) {
            std::ostringstream pair;
            pair << std::hex << std::setfill('0') << std::setw(2) << (i + 1);
            MiniData leaf("0x" + repeat(pair.str(), 32));
            MiniNumber sum(i);
            auto data = MMRData::CreateMMRDataLeafNode(leaf, sum);
            mmr.addEntry(*data);
        }
        mmr.finalizeSet();
        auto root = mmr.getRoot();
        std::cout << "mmrroot\t" << root->getData().to0xString() << "\n";
    } catch (const std::exception&) {
        std::cout << "mmrroot\tERROR\n";
    }

    return 0;
}
