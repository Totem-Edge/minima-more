/**
 * test_signtoken_coinid.cpp
 *
 * Regression test for `tokencreate signtoken:` verification.
 *
 * tokencreate signs the coin ID of the output that carries the token-create
 * descriptor. The descriptor's own `coinid` field is the 0x00 placeholder, so
 * verifying against that field (the previous tokenvalidate behaviour) always
 * fails for chain-created tokens. This test pins the actual signed message and
 * both verification outcomes.
 */

#include <iostream>
#include <string>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/keys/signature.hpp"
#include "org/minima/objects/keys/tree_key.hpp"

using org::minima::objects::Coin;
using org::minima::objects::Token;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniString;
using org::minima::objects::keys::Signature;
using org::minima::objects::keys::TreeKey;

static int g_pass = 0;
static int g_total = 0;

static void check(bool ok, const std::string& label) {
    g_total++;
    if (ok) g_pass++;
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << label << std::endl;
}

int main() {
    std::cout << "=== signtoken: signed message vs token descriptor coin ID ===" << std::endl;

    const MiniData seed(std::string(
        "0x1111111111111111111111111111111111111111111111111111111111111111"));
    // What tokencreate signs: transaction.calculateCoinID(firstInput, 0).
    const MiniData carryingCoinId(std::string(
        "0xabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcd"));

    TreeKey tk(seed, 64, 3);
    tk.setUses(1 * 64 + 2);
    const Signature sig = tk.sign(carryingCoinId);

    check(tk.verify(carryingCoinId, sig),
          "signature verifies against the carrying coin ID");

    Token created(
        Coin::COINID_OUTPUT,
        MiniNumber(44LL),   // scale = 44 - decimals(0)
        MiniNumber(1LL),    // totalSupply
        MiniString(std::string("{\"name\":\"Hex Seed 1\"}")),
        MiniString(std::string("RETURN TRUE")));

    check(created.getCoinID().isEqual(MiniData(Coin::COINID_OUTPUT)),
          "descriptor stores the 0x00 placeholder coin ID");
    check(!tk.verify(created.getCoinID(), sig),
          "signature does NOT verify against the placeholder");

    const MiniData* tokenId = created.getTokenID();
    check(tokenId != nullptr && !tokenId->isEqual(carryingCoinId),
          "token ID is independent of the carrying coin ID");

    // Embedding the signature changes the name, hence the token ID. That is why
    // the signature must be over the (name-independent) coin ID.
    const std::string signedName =
        "{\"name\":\"Hex Seed 1\",\"signtype\":\"minima\",\"signedby\":\"0x11\",\"signature\":\"0x22\"}";
    Token signedToken(
        Coin::COINID_OUTPUT,
        MiniNumber(44LL),
        MiniNumber(1LL),
        MiniString(signedName),
        MiniString(std::string("RETURN TRUE")));
    check(tokenId != nullptr && signedToken.getTokenID() != nullptr &&
              !tokenId->isEqual(*signedToken.getTokenID()),
          "signature fields change the token ID, so signing the token ID would be circular");

    std::cout << "\n=== " << g_pass << "/" << g_total << " checks passed ===" << std::endl;
    return (g_pass == g_total) ? 0 : 1;
}
