/**
 * test_consensus_vectors.cpp
 *
 * Consensus-critical golden vectors.
 *
 * Provenance:
 *   - GENESIS_COINID / TOKENID_MINIMA: hardcoded from the Java reference node.
 *   - All other vectors are frozen from this implementation as REGRESSION
 *     guards. They must be cross-checked against the Java reference node
 *     before mainnet launch (see V1 checklist).
 *
 * A change in any of these means a consensus break.
 */

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "org/minima/objects/address.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/keys/winternitz.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/system/genesis/genesis_coin.hpp"
#include "org/minima/system/genesis/genesis_m_m_r.hpp"
#include "org/minima/utils/crypto.hpp"

using org::minima::objects::Address;
using org::minima::objects::Coin;
using org::minima::objects::Token;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::keys::Winternitz;
using org::minima::objects::mmr::MMR;
using org::minima::system::genesis::GenesisCoin;
using org::minima::system::genesis::GenesisMMR;
using org::minima::utils::Crypto;

static int gPassed = 0;
static int gFailed = 0;

static void check(const std::string& name, const std::string& got, const std::string& expected) {
    bool ok = (got == expected);
    std::cout << "  " << name << ": " << (ok ? "PASS" : "FAIL") << std::endl;
    if (!ok) {
        std::cout << "    got: " << got << "\n    exp: " << expected << std::endl;
    }
    ok ? gPassed++ : gFailed++;
}

int main() {
    std::cout << "=== Consensus Golden Vector Verification ===" << std::endl;

    // --- Genesis coin ID (frozen from Java) ---
    {
        std::cout << "\n--- Genesis Coin ID ---" << std::endl;
        check("GENESIS_COINID",
              GenesisCoin::GENESIS_COINID.to0xString(),
              "0x5350415254414355534C4F5645534D494E494D41");
        check("TOKENID_MINIMA",
              Token::TOKENID_MINIMA.to0xString(),
              "0x00");
    }

    // --- Genesis MMR root ---
    {
        std::cout << "\n--- Genesis MMR Root ---" << std::endl;
        GenesisMMR mmr;
        auto root = mmr.getRoot();
        assert(root && "genesis MMR must have a root");
        check("GENESIS_MMR_ROOT", root->getData().to0xString(),
              "0xE60E583B6D5C8B951F9B8066EA53F980DBA84325DB9B007DF2262A9718495415");
    }

    // --- TRUE address ---
    {
        std::cout << "\n--- TRUE Address ---" << std::endl;
        const Address& trueAddr = Address::getTrueAddress();
        check("TRUE_SCRIPT", trueAddr.getScript(), "RETURN TRUE");
        check("TRUE_ADDRESS_DATA", trueAddr.getAddressData().to0xString(),
              "0x582AA40EA996419A3D8A5D26039A4E9584B746C2C26F860DAC8372BEA4457AF6");
        check("TRUE_MX_ADDRESS", trueAddr.getMinimaAddress(),
              "MxG082Z5AW0TACM86D3R2WT4Z1PKJKYGWRKDGM2DU30RB43EAVA8HBQURHCQE9V");
    }

    // --- Coin ID derivation ---
    {
        std::cout << "\n--- Coin ID Derivation ---" << std::endl;
        // calculateCoinID(base, output) = SHA3-256(serialize(base) || serialize(MiniNumber(output)))
        // Use the genesis coin ID as base, output 0 (the genesis output coin).
        MiniData base("0x5350415254414355534C4F5645534D494E494D41");
        MiniNumber out0(0);
        MiniNumber out1(1);
        MiniData cid0 = Crypto::getInstance().hashObjects(base, out0);
        MiniData cid1 = Crypto::getInstance().hashObjects(base, out1);
        check("COINID_GENESIS_OUTPUT_0", cid0.to0xString(),
              "0x511CA2F83A66EA6037673D1FAF2531F1D3463E2C59ED6B50B14144916ED848FD");
        check("COINID_GENESIS_OUTPUT_1", cid1.to0xString(),
              "0xEB0D9C254940EDE7574D7AE6D7F6A8428A7CDE1DE1C46822A83A198A46162909");
    }

    // --- WOTS round trip ---
    {
        std::cout << "\n--- WOTS Sign/Verify ---" << std::endl;
        MiniData seed("0x0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20");
        Winternitz wots(seed);
        MiniData pub = wots.getPublicKey();
        check("WOTS_PUBLIC_KEY", pub.to0xString(),
              "0xA8F6D0F5D949190144CA9E6977E8ED2ED7FB29F1E6194629CFF18EBA26F56557");

        std::string msg = "test message for WOTS signing";
        MiniData msgHash(Crypto::getInstance().hashData(
            std::vector<uint8_t>(msg.begin(), msg.end())));
        check("WOTS_MESSAGE_HASH", msgHash.to0xString(),
              "0x6DF0548C122D86350154A68F0CF4E818471447FFBB070F4CAFA368FA94C37CA4");

        MiniData sig = wots.sign(msgHash);
        check("WOTS_SIGNATURE", sig.to0xString(),
              "0xEE46EC0F7F965A0FE922A158DF2F5E8063EB327F52EFE092595424C126139557F884C1C5CCB7536B047C4F0523000217CE2323557678382A5B78C8390220B3A06E8CEB8564B98A1DBDF86067B8BCCE28CD3082199CE35EDDC0049E7BA193E3AF12F7A383EA259B5AEDA3AC0FCD5A3CCCF5C37B9CC5CF656C6B047AE7CC9D8AEB60CC5C34CB21970058A64FE783C4A9126F9EF38813768B903E4C5033CD50EADED52AC3EBFDC0CBFD4CADD90ED3F597A126B652921F156ABB2585E84C7784F7F36B447B8B2AF771313E505BBB2E309D2786AB90C82988EF540B3F56480D6BF5043F9BD5BA150C91D0EE31839B3AEAC3A20B1E7013925D9B747840CB251BC48186EADB4E6FAD91D80E72A736E67DF5D5C9DCE1FA9EF5CC0B9CE8A29E8269EBC24942A25AA7452C4F9DE24560E3298977AD456E339B8B29D7B6021E6EF1CC138C91A2244293FEAF3A009C4CA37BCA2DC2A7DE3619787D60BC7E5E62B64ACE87943421C09CC54339BB1E641C780E7B344A933A2863FE2550746D8CF857A12D9B456FCE74274F5DDE98D432B8AAC3F27D341ECAFF0CE3070A14F479D56CE78997CD9ABED18CF4EF28528E2FD6EE105386893004CC41BA6EB64F4A1936E86544513022BD97CFBD9BD6819E5A7F05A24BBF95175286E972541FDB8AE95F3494C7F1C3CD3AEE348322A8BE9AE58E8999C5B1CA86995318C28ACB484934A160913ED27E406C5BE430F54ADB5B0B910ADA8DF2F4C7E84DC7E59AEBAA4F2FA6D3D889CE2465FD8488F859A1948692E16454D5D0AB3001BA3688AC1A6C098B2A4A22AB3E1B68EF78B2C734841A325B0EADD5E972FF4A7C009A20F791B00F8335877FFD850B7058AF48D166CC86E409893D130C7C8CE9F593A5E77A8ECFE66048BD14C9BA43CE6A2F641E977F1D27C081A450A313D43CA55152B9AA97863EE9D1E267D69F52D92EFFAA704275C26200C326F4081C472CD988CEE5F5937DEB0D176AF05164B42101DFF07FDAEDFA8D0734735FAC7D21D9E50EDB28558FDC6AF61068BAE90B864EFDB58107F485E8AF50687B2FDB1B8E9BB9394BB1136D8A941A50838CCD2D2A917CC37B615CAC7FFC0D78F9B8EDD30F4A3D2CBBA0C5DDAF30157B00F4FDA5A6311279417A08B462422C85682CE8BF13F440C2EFB829B65FBC77F1EB72B3B219194E37524A2CF3CC9FB271D9C4716DD1E9CAEF986DAFAE5AEBF9DEB78F7F652C5404E15E647A69671352A6600D82B20ECD0E7B61E665FFCA70BE406E4093656A77793B886874EF4A4487ED491147D4DC788CC40BCF38C8EA01FD4A0C0C69B0857E519F16382564E84F87A115D709024915179CAF824B8E6141078ECEC6C7F88E441F9108E885F90E86C2623EE470E7A6E749341B96FADB15C3CD8F19107FFAEDA3286B6A031F109DB4D880020B5A67E8531F9B7EF375464D06ADAEAB51E456B7973975C4248B3A8C2C459F02F894CFD00B53B47384F3D2A951958CDF895DBE60DC0C17B6EB547879678ABE2D2DC63C594259D87E2717698923DEDB14225D449F86");
        bool ok = Winternitz::verify(pub, msgHash, sig);
        std::cout << "  WOTS_VERIFY: " << (ok ? "PASS" : "FAIL") << std::endl;
        ok ? gPassed++ : gFailed++;
    }

    std::cout << "\n=== " << gPassed << "/" << (gPassed + gFailed) << " consensus vectors passed ===" << std::endl;
    return gFailed == 0 ? 0 : 1;
}
