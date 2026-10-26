#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/tokens/script_tokenizer.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_data.hpp"

#include <cstdio>
#include <string>

using org::minima::kissvm::Contract;
using org::minima::kissvm::values::HexValue;
using org::minima::kissvm::tokens::ScriptTokenizer;
using org::minima::objects::base::MiniNumber;

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        if (!ok) {
            std::printf("FAIL %s\n", what);
            ++failures;
        }
    };

    check(HexValue(MiniNumber(255)).getMiniData().to0xString() == "0xFF", "HEX(255) == 0xFF");
    check(HexValue(MiniNumber(128)).getMiniData().to0xString() == "0x80", "HEX(128) == 0x80");
    check(HexValue(MiniNumber(0)).getMiniData().to0xString() == "0x00", "HEX(0) == 0x00");
    check(HexValue(MiniNumber(1)).getMiniData().to0xString() == "0x01", "HEX(1) == 0x01");
    check(HexValue(MiniNumber(256)).getMiniData().to0xString() == "0x0100", "HEX(256) == 0x0100");

    check(!ScriptTokenizer::isHex("0X01"), "uppercase 0X is not Java hex");
    check(ScriptTokenizer::isHex("0x01"), "lowercase 0x is hex");

    check(MiniNumber("4294967296").getAsInt() == 0, "getAsInt wraps to Java low 32 bits");
    bool limitedThrew = false;
    try { MiniNumber tooBig("18446744073709551616"); } catch (...) { limitedThrew = true; }
    check(limitedThrew, "normal MiniNumber rejects above Java 2^64 bound");
    bool wideOk = true;
    try { (void)MiniNumber::fromWideString("18446744073709551616"); } catch (...) { wideOk = false; }
    check(wideOk, "wide MiniNumber accepts chain weight above Java bound");

    const std::string input = "LET a=1/3 RETURN TRUE";
    const std::string clean = Contract::cleanScript(input);
    check(clean == "LET a=1/3 RETURN TRUE", "Java-compatible cleanScript spacing");

    const std::string longHex = "LET a=0x" + std::string(12000, '0') + " RETURN TRUE";
    const std::string longClean = Contract::cleanScript(longHex);
    check(!longClean.empty(), "long HEX script does not crash");

    std::printf("clean=%s\nlong_clean_len=%zu\n", clean.c_str(), longClean.size());
    std::printf("RESULT %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
