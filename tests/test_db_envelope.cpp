// Verifies the on-disk database envelope shared with hawk-node.
//
// The container is deliberately identical in both forks, so this test pins the
// exact byte layout (magic, version, kind, length, CRC32, trailer) and the
// fail-closed load behaviour that relies on it.

#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/streamable.hpp"

#include <zlib.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool condition, const std::string& what) {
    if (!condition) {
        std::cerr << "FAIL: " << what << std::endl;
        ++g_failures;
    }
}

class Blob final : public org::minima::utils::Streamable {
public:
    std::string payload;

    void writeDataStream(std::ostream& out) override {
        out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    }

    void readDataStream(std::istream& in) override {
        std::ostringstream oss;
        oss << in.rdbuf();
        payload = oss.str();
    }
};

std::vector<std::uint8_t> readFile(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in),
                                     std::istreambuf_iterator<char>());
}

void writeFile(const fs::path& file, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
}

std::uint32_t readU32(const std::vector<std::uint8_t>& bytes, std::size_t at) {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(bytes.at(at + i)) << (i * 8);
    }
    return value;
}

std::uint64_t readU64(const std::vector<std::uint8_t>& bytes, std::size_t at) {
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<std::uint64_t>(bytes.at(at + i)) << (i * 8);
    }
    return value;
}

std::uint32_t crc32Of(const std::string& bytes) {
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, reinterpret_cast<const Bytef*>(bytes.data()),
                static_cast<uInt>(bytes.size()));
    return static_cast<std::uint32_t>(crc);
}

bool throws(const std::function<void()>& fn) {
    try {
        fn();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

} // namespace

int main() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path dir = fs::temp_directory_path() / ("mm-db-envelope-" + std::to_string(stamp));
    fs::create_directories(dir);

    const std::string payload = "cascade-and-chaintree-payload-\x01\x02\x03";
    const fs::path db = dir / "cascade.db";

    // 1. Write through the envelope and pin the header layout.
    {
        Blob blob;
        blob.payload = payload;
        org::minima::utils::MiniFile::saveObjectAtomic(db, blob, 2);
    }

    const auto bytes = readFile(db);
    check(bytes.size() == 32 + payload.size() + 8, "file size is header+payload+trailer");
    check(std::memcmp(bytes.data(), "HAWKDB01", 8) == 0, "magic is HAWKDB01");
    check(readU32(bytes, 8) == 1, "envelope version is 1");
    check(readU32(bytes, 12) == 2, "kind is cascade (2)");
    check(readU64(bytes, 16) == payload.size(), "length matches payload");
    check(readU32(bytes, 24) == crc32Of(payload), "crc32 matches zlib crc32 of payload");
    check(std::memcmp(bytes.data() + bytes.size() - 8, "HAWKEND1", 8) == 0,
          "trailer is HAWKEND1");

    // 2. Round trip.
    {
        Blob blob;
        org::minima::utils::MiniFile::loadObjectVerified(db, blob, 2);
        check(blob.payload == payload, "payload round trips");
    }

    // 3. Wrong kind is rejected.
    {
        Blob blob;
        check(throws([&] { org::minima::utils::MiniFile::loadObjectVerified(db, blob, 1); }),
              "wrong kind is rejected");
    }

    // 4. Truncation is rejected before deserialization.
    {
        auto cut = bytes;
        cut.resize(cut.size() - 1);
        const fs::path truncated = dir / "truncated.db";
        writeFile(truncated, cut);
        Blob blob;
        check(throws([&] { org::minima::utils::MiniFile::loadObjectVerified(truncated, blob, 2); }),
              "truncated file is rejected");
    }

    // 5. A flipped payload byte is caught by the checksum, then quarantined.
    {
        auto flipped = bytes;
        flipped.at(32 + 3) ^= 0x40;
        const fs::path corrupt = dir / "corrupt.db";
        writeFile(corrupt, flipped);
        Blob blob;
        check(throws([&] { org::minima::utils::MiniFile::loadObjectVerified(corrupt, blob, 2); }),
              "checksum mismatch is rejected");

        org::minima::utils::MiniFile::quarantineFile(corrupt, "checksum mismatch");
        check(!fs::exists(corrupt), "corrupt file is moved out of the way");
        bool quarantined = false;
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.path().filename().string().rfind("corrupt.db.corrupt-", 0) == 0) {
                quarantined = true;
            }
        }
        check(quarantined, "quarantine keeps a timestamped copy");
    }

    // 6. Legacy raw files are only readable when explicitly requested.
    {
        const fs::path legacy = dir / "legacy.db";
        Blob blob;
        blob.payload = payload;
        org::minima::utils::MiniFile::saveObjectDirect(legacy, blob);

        Blob strict;
        check(throws([&] { org::minima::utils::MiniFile::loadObjectVerified(legacy, strict, 2); }),
              "legacy raw file is rejected when strict");

        Blob legacyRead;
        org::minima::utils::MiniFile::loadObjectVerified(legacy, legacyRead, 2, true);
        check(legacyRead.payload == payload, "legacy raw file loads when allowed");
    }

    fs::remove_all(dir);

    if (g_failures != 0) {
        std::cerr << g_failures << " check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "db envelope: all checks passed" << std::endl;
    return 0;
}
