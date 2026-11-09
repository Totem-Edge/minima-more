/**
 * fuzz_driver.cpp
 *
 * Portable driver for the LLVMFuzzerTestOneInput targets when libFuzzer is not
 * available (e.g. AppleClang ships no fuzzer runtime). It runs a corpus and can
 * run a deterministic mutation loop, so sanitizer builds still get useful
 * coverage in CI.
 *
 * Interface deliberately mirrors the libFuzzer subset used by scripts:
 *   fuzz_<target> [-runs=N] FILE_OR_DIR...
 *
 * Without -runs, each input is executed once (corpus replay).
 * With -runs=N, each seed gets N deterministic mutations (fixed seed).
 */

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

namespace {

std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
}

void collect(const std::string& path, std::vector<std::string>& out) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (fs::is_directory(path, ec)) {
        for (const auto& entry : fs::directory_iterator(path, ec)) {
            if (entry.is_regular_file(ec)) {
                out.push_back(entry.path().string());
            }
        }
    } else {
        out.push_back(path);
    }
}

void mutate(std::vector<uint8_t>& buf, std::mt19937& rng) {
    if (buf.empty()) {
        buf.push_back(static_cast<uint8_t>(rng()));
    }
    const int ops = 1 + static_cast<int>(rng() % 8);
    for (int i = 0; i < ops; ++i) {
        switch (rng() % 4) {
            case 0: {  // bit flip
                size_t pos = rng() % buf.size();
                buf[pos] ^= static_cast<uint8_t>(1u << (rng() % 8));
                break;
            }
            case 1: {  // byte set
                size_t pos = rng() % buf.size();
                buf[pos] = static_cast<uint8_t>(rng());
                break;
            }
            case 2: {  // insert
                size_t pos = rng() % (buf.size() + 1);
                buf.insert(buf.begin() + static_cast<long>(pos),
                           static_cast<uint8_t>(rng()));
                break;
            }
            default: {  // erase
                if (buf.size() > 1) {
                    size_t pos = rng() % buf.size();
                    buf.erase(buf.begin() + static_cast<long>(pos));
                }
                break;
            }
        }
        if (buf.size() > 4 * 1024 * 1024) {
            buf.resize(4 * 1024 * 1024);
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s [-runs=N] FILE_OR_DIR...\n", argv[0]);
        return 2;
    }

    long long runs_per_seed = 0;
    std::vector<std::string> paths;
    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "-runs=", 6) == 0) {
            runs_per_seed = std::atoll(argv[i] + 6);
        } else {
            collect(argv[i], paths);
        }
    }
    std::sort(paths.begin(), paths.end());

    std::mt19937 rng(0xC0FFEEu);  // fixed seed: reproducible
    long long runs = 0;

    for (const auto& path : paths) {
        const std::vector<uint8_t> seed = readFile(path);
        LLVMFuzzerTestOneInput(seed.data(), seed.size());
        ++runs;

        std::vector<uint8_t> buf = seed;
        for (long long i = 0; i < runs_per_seed; ++i) {
            mutate(buf, rng);
            LLVMFuzzerTestOneInput(buf.data(), buf.size());
            ++runs;
            if ((i & 0x3FF) == 0) {
                buf = seed;  // drift back to the seed periodically
            }
        }
    }

    std::fprintf(stderr, "fuzz_driver: %lld inputs processed from %zu seeds\n",
                 runs, paths.size());
    return 0;
}
