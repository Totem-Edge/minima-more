/**
 * test_serialization.cpp
 *
 * Standalone serialization parity test.
 * Generates hex strings for C++ serialization of MiniNumber, MiniData,
 * MiniByte, and MiniString so they can be compared byte-for-byte against
 * Java golden vectors.
 *
 * Build: cmake --build <build-dir> --target test_serialization
 * Run:   ./test_serialization
 * Compare output against Java GoldenVectorRunner output.
 */

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <cstdint>

#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_byte.hpp"
#include "org/minima/objects/base/mini_string.hpp"

using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniByte;
using org::minima::objects::base::MiniString;

// -------------------------------------------------------------------
// Utility: write an object's writeDataStream to a hex string
// -------------------------------------------------------------------
template<typename T>
std::string hex_from_stream(T& obj) {
    std::ostringstream oss(std::ios::binary);
    obj.writeDataStream(oss);
    std::ostringstream hex;
    for (char c : oss.str()) {
        hex << std::hex << std::setfill('0') << std::setw(2)
            << (static_cast<unsigned int>(static_cast<unsigned char>(c)));
    }
    return hex.str();
}

static std::string hex_from_data(const MiniData& d) {
    std::ostringstream hex;
    const auto& raw = d.getBytes();
    for (unsigned char c : raw) {
        hex << std::hex << std::setfill('0') << std::setw(2)
            << static_cast<unsigned int>(c);
    }
    return hex.str();
}

struct NumCase {
    std::string label;
    MiniNumber num;
};

int main() {
    std::cout << "=== C++ Serialization Golden Vectors ===" << std::endl;
    std::cout << std::endl;

    // =====================================================================
    // 1. MiniNumber writeDataStream
    // =====================================================================
    std::cout << "--- 1. MiniNumber writeDataStream ---" << std::endl;
    std::vector<NumCase> nums = {
        {"0",               MiniNumber(0LL)},
        {"1",               MiniNumber(1LL)},
        {"-1",              MiniNumber(-1LL)},
        {"255",             MiniNumber(255LL)},
        {"256",             MiniNumber(256LL)},
        {"65535",           MiniNumber(65535LL)},
        {"65536",           MiniNumber(65536LL)},
        {"MAX",             MiniNumber(std::string("18446744073709551615"))},
        {"0.1",             MiniNumber(std::string("0.1"))},
        {"-100",            MiniNumber(-100LL)},
        {"12345678901234567890", MiniNumber(std::string("12345678901234567890"))},
    };
    for (auto& nc : nums) {
        std::cout << "  MiniNumber(" << nc.label << ") = " << hex_from_stream(nc.num) << std::endl;
    }
    std::cout << std::endl;

    // =====================================================================
    // 2. MiniData writeDataStream
    // =====================================================================
    std::cout << "--- 2. MiniData writeDataStream ---" << std::endl;
    std::vector<MiniData> datas = {
        MiniData("0x"),
        MiniData("0x00"),
        MiniData("0x01"),
        MiniData("0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00"),
    };
    for (auto& d : datas) {
        std::cout << "  MiniData(" << hex_from_data(d) << ") = " << hex_from_stream(d) << std::endl;
    }
    std::cout << std::endl;

    // =====================================================================
    // 3. MiniData writeHashToStream
    // =====================================================================
    std::cout << "--- 3. MiniData writeHashToStream ---" << std::endl;
    for (auto& d : datas) {
        std::ostringstream oss(std::ios::binary);
        d.writeHashToStream(oss);
        std::ostringstream hex;
        for (char c : oss.str()) {
            hex << std::hex << std::setfill('0') << std::setw(2)
                << (static_cast<unsigned int>(static_cast<unsigned char>(c)));
        }
        std::cout << "  Hash(" << hex_from_data(d) << ") = " << hex.str() << std::endl;
    }
    std::cout << std::endl;

    // =====================================================================
    // 4. MiniByte writeDataStream
    // =====================================================================
    std::cout << "--- 4. MiniByte writeDataStream ---" << std::endl;
    auto* bytes = new MiniByte[3]{MiniByte(true), MiniByte(false), MiniByte(42)};
    for (int i = 0; i < 3; ++i) {
        std::cout << "  MiniByte(" << (i == 0 ? "TRUE" : i == 1 ? "FALSE" : "42") << ") = " << hex_from_stream(bytes[i]) << std::endl;
    }
    delete[] bytes;
    std::cout << std::endl;

    // =====================================================================
    // 5. MiniString writeDataStream
    // =====================================================================
    std::cout << "--- 5. MiniString writeDataStream ---" << std::endl;
    MiniString strs[] = {
        MiniString(""),
        MiniString("hello"),
        MiniString("{\"json\": true}"),
        MiniString("0x1234"),
    };
    for (auto& s : strs) {
        std::ostringstream oss(std::ios::binary);
        s.writeDataStream(oss);
        std::ostringstream hex;
        for (char c : oss.str()) {
            hex << std::hex << std::setfill('0') << std::setw(2)
                << (static_cast<unsigned int>(static_cast<unsigned char>(c)));
        }
        std::cout << "  MiniString(\"" << s.toString() << "\") = " << hex.str() << std::endl;
    }
    std::cout << std::endl;

    // =====================================================================
    // Done
    // =====================================================================
    std::cout << "=== Done ===" << std::endl;
    return 0;
}
