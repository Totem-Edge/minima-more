/**
 * test_serialization_standalone.cpp
 *
 * Truly standalone serialization parity test.
 * Duplicates the serialization logic for MiniNumber, MiniData, MiniByte,
 * and MiniString from first principles so it can compile without any
 * project dependencies.
 *
 * Run: c++ -std=c++17 -o test_serialization_standalone test_serialization_standalone.cpp && ./test_serialization_standalone
 */

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <climits>
#include <stdexcept>
#include <algorithm>

// ============================================================================
// Standalone reimplementation of MiniNumber serialization
// ============================================================================
class MiniNumber {
public:
    MiniNumber() : mScale(0), mValue("0") {}

    explicit MiniNumber(int64_t v) : mScale(0) {
        mValue = std::to_string(v);
    }

    explicit MiniNumber(const std::string& v) {
        // Parse scale from decimal notation
        auto dot = v.find('.');
        if (dot != std::string::npos) {
            mScale = static_cast<int8_t>(v.length() - dot - 1);
            std::string intPart = v.substr(0, dot);
            std::string fracPart = v.substr(dot + 1);
            // Remove trailing zeros from frac part
            while (!fracPart.empty() && fracPart.back() == '0') {
                fracPart.pop_back();
            }
            if (fracPart.empty()) {
                mValue = intPart;
                mScale = 0;
            } else {
                mValue = intPart + fracPart;
            }
        } else {
            mScale = 0;
            mValue = v;
        }
        // Handle negative sign
        if (!mValue.empty() && mValue[0] == '-') {
            mIsNegative = true;
        } else {
            mIsNegative = false;
        }
    }

    void writeDataStream(std::ostream& out) const {
        // Serialize as: scale_byte | twos_complement_bytes
        out.put(static_cast<char>(mScale));

        // Convert absolute value to big-endian bytes
        std::string absStr = (mValue[0] == '-') ? mValue.substr(1) : mValue;
        // Convert decimal string to bytes (big-endian)
        std::vector<uint8_t> bytes = decimalToBE(absStr);

        if (bytes.empty()) {
            bytes.push_back(0);
        }

        // Two's complement for negative numbers
        if (mIsNegative && !bytes.empty()) {
            twosComplement(bytes);
        }

        // Write length as single byte, then data bytes
        out.put(static_cast<char>(bytes.size() & 0xFF));
        out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

private:
    static std::vector<uint8_t> decimalToBE(const std::string& dec) {
        if (dec == "0") return {0};
        std::vector<uint8_t> result;
        std::string val = dec;
        while (!val.empty() && val != "0") {
            int remainder = 0;
            std::string next;
            for (char c : val) {
                int digit = c - '0';
                int current = remainder * 10 + digit;
                int q = current / 2;
                remainder = current % 2;
                if (!next.empty() || q != 0) {
                    next.push_back(static_cast<char>('0' + q));
                }
            }
            result.push_back(static_cast<uint8_t>(remainder));
            val = next;
        }
        std::reverse(result.begin(), result.end());
        return result;
    }

    static void twosComplement(std::vector<uint8_t>& bytes) {
        // Invert all bits
        for (auto& b : bytes) {
            b = ~b;
        }
        // Add 1
        for (int i = static_cast<int>(bytes.size()) - 1; i >= 0; --i) {
            bytes[i]++;
            if (bytes[i] != 0) break;
        }
    }

    int8_t mScale;
    std::string mValue;
    bool mIsNegative = false;
};

// ============================================================================
// Standalone reimplementation of MiniData serialization
// ============================================================================
class MiniData {
public:
    MiniData() {}

    explicit MiniData(const std::string& hex) {
        if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
            std::string raw = hex.substr(2);
            if (!raw.empty()) {
                if (raw.size() % 2 != 0) {
                    raw = "0" + raw;
                }
                for (size_t i = 0; i < raw.size(); i += 2) {
                    std::string byteStr = raw.substr(i, 2);
                    uint8_t byte = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
                    mData.push_back(byte);
                }
            }
        }
    }

    void writeDataStream(std::ostream& out) const {
        int32_t len = static_cast<int32_t>(mData.size());
        // Big-endian int32 length prefix
        out.put(static_cast<char>((len >> 24) & 0xFF));
        out.put(static_cast<char>((len >> 16) & 0xFF));
        out.put(static_cast<char>((len >> 8) & 0xFF));
        out.put(static_cast<char>(len & 0xFF));
        if (len > 0) {
            out.write(reinterpret_cast<const char*>(mData.data()), len);
        }
    }

    void writeHashToStream(std::ostream& out) const {
        // Same as writeDataStream but with hash length (32 bytes)
        int32_t len = static_cast<int32_t>(mData.size());
        // Pad or truncate to 32 bytes
        std::vector<uint8_t> hashData(32, 0);
        if (len <= 32) {
            std::copy(mData.begin(), mData.end(), hashData.end() - len);
        } else {
            std::copy(mData.end() - 32, mData.end(), hashData.begin());
        }
        out.write(reinterpret_cast<const char*>(hashData.data()), 32);
    }

    std::string toString() const {
        std::ostringstream hex;
        for (uint8_t b : mData) {
            hex << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
        }
        return hex.str();
    }

private:
    std::vector<uint8_t> mData;
};

// ============================================================================
// Standalone reimplementation of MiniByte serialization
// ============================================================================
class MiniByte {
public:
    MiniByte() : mValue(0) {}
    explicit MiniByte(bool v) : mValue(v ? 1 : 0) {}
    explicit MiniByte(uint8_t v) : mValue(v) {}

    void writeDataStream(std::ostream& out) const {
        out.put(static_cast<char>(mValue));
    }

private:
    uint8_t mValue;
};

// ============================================================================
// Standalone reimplementation of MiniString serialization
// ============================================================================
class MiniString {
public:
    MiniString() {}
    explicit MiniString(const std::string& s) : mString(s) {}

    void writeDataStream(std::ostream& out) const {
        int32_t len = static_cast<int32_t>(mString.size());
        // Same as MiniData: int32 BE length prefix + raw bytes
        out.put(static_cast<char>((len >> 24) & 0xFF));
        out.put(static_cast<char>((len >> 16) & 0xFF));
        out.put(static_cast<char>((len >> 8) & 0xFF));
        out.put(static_cast<char>(len & 0xFF));
        if (len > 0) {
            out.write(mString.data(), len);
        }
    }

    std::string toString() const { return mString; }

private:
    std::string mString;
};

// ============================================================================
// Utility: convert stream content to hex string
// ============================================================================
std::string hex_from_stream(std::ostringstream& oss) {
    std::ostringstream hex;
    for (char c : oss.str()) {
        hex << std::hex << std::setfill('0') << std::setw(2)
            << (static_cast<unsigned int>(static_cast<unsigned char>(c)));
    }
    return hex.str();
}

int main() {
    std::cout << "=== C++ Standalone Serialization Golden Vectors ===" << std::endl;
    std::cout << std::endl;

    // =====================================================================
    // 1. MiniNumber writeDataStream
    // =====================================================================
    std::cout << "--- 1. MiniNumber writeDataStream ---" << std::endl;
    std::vector<std::pair<std::string, MiniNumber>> nums = {
        {"0",               MiniNumber(0LL)},
        {"1",               MiniNumber(1LL)},
        {"-1",              MiniNumber(-1LL)},
        {"255",             MiniNumber(255LL)},
        {"256",             MiniNumber(256LL)},
        {"65535",           MiniNumber(65535LL)},
        {"65536",           MiniNumber(65536LL)},
        {"MAX",             MiniNumber(std::string("18446744073709551615"))},
        {"MIN",             MiniNumber(std::string("-18446744073709551616"))},
        {"0.1",             MiniNumber(std::string("0.1"))},
        {"-100",            MiniNumber(-100LL)},
        {"12345678901234567890", MiniNumber(std::string("12345678901234567890"))},
    };
    for (auto& nc : nums) {
        std::ostringstream oss(std::ios::binary);
        nc.second.writeDataStream(oss);
        std::cout << "  MiniNumber(" << nc.first << ") = " << hex_from_stream(oss) << std::endl;
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
        std::ostringstream oss(std::ios::binary);
        d.writeDataStream(oss);
        std::cout << "  MiniData(" << d.toString() << ") = " << hex_from_stream(oss) << std::endl;
    }
    std::cout << std::endl;

    // =====================================================================
    // 3. MiniData writeHashToStream
    // =====================================================================
    std::cout << "--- 3. MiniData writeHashToStream ---" << std::endl;
    for (auto& d : datas) {
        std::ostringstream oss(std::ios::binary);
        d.writeHashToStream(oss);
        std::cout << "  Hash(" << d.toString() << ") = " << hex_from_stream(oss) << std::endl;
    }
    std::cout << std::endl;

    // =====================================================================
    // 4. MiniByte writeDataStream
    // =====================================================================
    std::cout << "--- 4. MiniByte writeDataStream ---" << std::endl;
    auto* bytes = new MiniByte[3]{MiniByte(true), MiniByte(false), MiniByte(static_cast<uint8_t>(42))};
    for (int i = 0; i < 3; ++i) {
        std::ostringstream oss(std::ios::binary);
        bytes[i].writeDataStream(oss);
        std::cout << "  MiniByte(" << (i == 0 ? "TRUE" : i == 1 ? "FALSE" : "42") << ") = " << hex_from_stream(oss) << std::endl;
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
        std::cout << "  MiniString(\"" << s.toString() << "\") = " << hex_from_stream(oss) << std::endl;
    }
    std::cout << std::endl;

    // =====================================================================
    // Done
    // =====================================================================
    std::cout << "=== Done ===" << std::endl;
    return 0;
}
