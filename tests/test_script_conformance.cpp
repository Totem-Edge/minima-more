#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/tokens/script_tokenizer.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/state_variable.hpp"

#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

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

    // Java parity: BigDecimal.setScale(44, DOWN) keeps scale 44 even when the
    // value rounds to zero, and that scale is serialized on the wire.
    {
        MiniNumber tiny("1E-45");
        std::ostringstream oss;
        tiny.writeDataStream(oss);
        const std::string wire = oss.str();
        const bool wireOk = wire.size() == 3 &&
                            static_cast<unsigned char>(wire[0]) == 0x2C &&
                            static_cast<unsigned char>(wire[1]) == 0x01 &&
                            static_cast<unsigned char>(wire[2]) == 0x00;
        check(wireOk, "1E-45 serializes as Java 0E-44 (scale byte 44)");
        check(tiny.decimalPlaces() == 44, "1E-45 keeps scale 44");
        check(tiny.toString() == "0", "1E-45 prints as 0");

        auto wireOf = [](const MiniNumber& m) {
            std::ostringstream out;
            MiniNumber copy = m;
            copy.writeDataStream(out);
            return out.str();
        };
        check(wireOf(MiniNumber("0.0")) == std::string("\x01\x01\x00", 3),
              "0.0 preserves scale 1 on the wire");
        check(wireOf(MiniNumber("0E+5")) == std::string("\xFB\x01\x00", 3),
              "0E+5 preserves negative scale on the wire");
    }

    // Hostile exponents must not trigger unbounded pow10 work.
    {
        bool hugeNegOk = true;
        try {
            MiniNumber hugeNeg("1E-999999999");
            hugeNegOk = (hugeNeg.toString() == "0" && hugeNeg.decimalPlaces() == 44);
        } catch (...) {
            hugeNegOk = false;
        }
        check(hugeNegOk, "huge negative exponent terminates as zero");

        bool hugePosRejected = false;
        try {
            (void)MiniNumber("1E+2147483647");
        } catch (...) {
            hugePosRejected = true;
        }
        check(hugePosRejected, "huge positive exponent rejected without expansion");
    }

    auto eval = [](const std::string& script, bool& parseOK, bool& success) {
        org::minima::objects::Witness w;
        org::minima::objects::Transaction tx;
        std::vector<std::unique_ptr<org::minima::objects::StateVariable>> st;
        Contract c(script, "", w, tx, std::move(st));
        c.run();
        parseOK = c.isParseOK();
        success = c.isSuccess();
    };

    // Java allows exactly 64 nested expression brackets (and rejects 65).
    {
        auto nested = [](int n, const char* body) {
            std::string s = "RETURN ";
            s.append(static_cast<std::size_t>(n), '(');
            s += body;
            s.append(static_cast<std::size_t>(n), ')');
            return s;
        };
        bool p64 = false, s64 = false, p65 = true, s65 = false;
        eval(nested(64, "TRUE"), p64, s64);
        eval(nested(65, "TRUE"), p65, s65);
        check(p64 && s64, "64 nested brackets accepted like Java");
        check(!p65, "65 nested brackets rejected like Java");
    }

    // LEN/SUBSTR use Java String semantics (UTF-16 code units, not UTF-8 bytes).
    {
        bool p = false, s = false;
        eval("RETURN LEN([\xC3\xA9]) EQ 1", p, s);          // e-acute: 1 BMP char
        check(p && s, "LEN counts UTF-16 units for BMP text");
        eval("RETURN LEN([\xF0\x9F\x98\x80]) EQ 2", p, s);  // emoji: 2 UTF-16 units
        check(p && s, "LEN counts surrogate pairs as two units");
        eval("RETURN SUBSTR(0 1 [\xC3\xA9]) EQ [\xC3\xA9]", p, s);
        check(p && s, "SUBSTR indexes UTF-16 units");
    }

    std::printf("clean=%s\nlong_clean_len=%zu\n", clean.c_str(), longClean.size());
    std::printf("RESULT %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
