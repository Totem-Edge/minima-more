// Standalone SHA3-256 known-answer test (Phase 0.6 GATE)
// Compares OpenSSL EVP_sha3_256 output against Python hashlib reference vectors
// (Python hashlib uses a NIST-validated SHA3-256 implementation)
//
// Build: g++ -std=c++17 -o test_sha3 test_sha3.cpp -I/opt/homebrew/opt/openssl@3/include -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto
// Run:   ./test_sha3

#include <openssl/evp.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>

static std::string to_hex(const unsigned char* data, size_t len) {
    static const char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; i++) {
        out.push_back(hex[(data[i] >> 4) & 0xF]);
        out.push_back(hex[data[i] & 0xF]);
    }
    return out;
}

static std::string sha3_256(const unsigned char* data, size_t len) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        std::cerr << "FAIL: EVP_MD_CTX_new returned null\n";
        std::exit(1);
    }
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;

    if (EVP_DigestInit_ex(ctx, EVP_sha3_256(), nullptr) != 1) {
        std::cerr << "FAIL: EVP_DigestInit_ex failed\n";
        EVP_MD_CTX_free(ctx);
        std::exit(1);
    }
    if (EVP_DigestUpdate(ctx, data, len) != 1) {
        std::cerr << "FAIL: EVP_DigestUpdate failed\n";
        EVP_MD_CTX_free(ctx);
        std::exit(1);
    }
    if (EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
        std::cerr << "FAIL: EVP_DigestFinal_ex failed\n";
        EVP_MD_CTX_free(ctx);
        std::exit(1);
    }
    EVP_MD_CTX_free(ctx);
    return to_hex(hash, hash_len);
}

static std::vector<unsigned char> hex_to_bytes(const std::string& hex) {
    std::vector<unsigned char> bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        bytes.push_back((unsigned char)std::stoi(hex.substr(i, 2), nullptr, 16));
    }
    return bytes;
}

int main() {
    int failures = 0;

    // Reference vectors from Python: python3 -c "import hashlib; print(hashlib.sha3_256(b'').hexdigest())" etc.
    // These are FIPS 202 SHA3-256 vectors (same as Bouncy Castle SHA3Digest(256))

    // Test 1: SHA3-256("")
    {
        unsigned char empty[1];
        std::string result = sha3_256(empty, 0);
        const std::string expected = "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a";
        if (result != expected) {
            std::cerr << "FAIL Test 1 (SHA3-256 of empty string):\n";
            std::cerr << "  Expected: " << expected << "\n";
            std::cerr << "  Got:      " << result << "\n";
            failures++;
        } else {
            std::cout << "PASS Test 1 (SHA3-256 of empty string)\n";
        }
    }

    // Test 2: SHA3-256("abc")
    {
        const unsigned char data[] = {'a', 'b', 'c'};
        std::string result = sha3_256(data, 3);
        const std::string expected = "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532";
        if (result != expected) {
            std::cerr << "FAIL Test 2 (SHA3-256 of \"abc\"):\n";
            std::cerr << "  Expected: " << expected << "\n";
            std::cerr << "  Got:      " << result << "\n";
            failures++;
        } else {
            std::cout << "PASS Test 2 (SHA3-256 of \"abc\")\n";
        }
    }

    // Test 3: SHA3-256(1,000,000 'a' chars)
    {
        std::vector<unsigned char> data(1000000, 'a');
        std::string result = sha3_256(data.data(), data.size());
        const std::string expected = "5c8875ae474a3634ba4fd55ec85bffd661f32aca75c6d699d0cdcb6c115891c1";
        if (result != expected) {
            std::cerr << "FAIL Test 3 (SHA3-256 of 1,000,000 'a'):\n";
            std::cerr << "  Expected: " << expected << "\n";
            std::cerr << "  Got:      " << result << "\n";
            failures++;
        } else {
            std::cout << "PASS Test 3 (SHA3-256 of 1,000,000 'a')\n";
        }
    }

    // Test 4: Chain hash determinism (1000 iterations)
    {
        const unsigned char seed[] = {'s', 'e', 'e', 'd'};
        std::string hash1, hash2;

        // Run 1
        {
            std::string h = sha3_256(seed, 4);
            auto current = hex_to_bytes(h);
            for (int i = 1; i < 1000; i++) {
                h = sha3_256(current.data(), current.size());
                current = hex_to_bytes(h);
            }
            hash1 = h;
        }

        // Run 2 (should be identical)
        {
            std::string h = sha3_256(seed, 4);
            auto current = hex_to_bytes(h);
            for (int i = 1; i < 1000; i++) {
                h = sha3_256(current.data(), current.size());
                current = hex_to_bytes(h);
            }
            hash2 = h;
        }

        if (hash1 != hash2) {
            std::cerr << "FAIL Test 4 (chain hash determinism):\n";
            std::cerr << "  Run 1: " << hash1 << "\n";
            std::cerr << "  Run 2: " << hash2 << "\n";
            failures++;
        } else {
            std::cout << "PASS Test 4 (chain hash 1000 iterations deterministic)\n";
            std::cout << "  Result: " << hash1 << "\n";
        }
    }

    // Test 5: Verify it's NOT Keccak-256 (original Keccak would give different result for empty string)
    // Keccak-256("") = c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470
    // SHA3-256("")   = a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a
    {
        unsigned char empty[1];
        std::string result = sha3_256(empty, 0);
        const std::string keccak_empty = "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470";
        if (result == keccak_empty) {
            std::cerr << "FAIL Test 5: OpenSSL EVP_sha3_256 produces Keccak-256, NOT FIPS 202 SHA3-256!\n";
            std::cerr << "  This means OpenSSL is using original Keccak padding (0x01) instead of FIPS 202 (0x06)\n";
            std::cerr << "  Java Bouncy Castle SHA3Digest uses FIPS 202 -- signatures will NOT match!\n";
            failures++;
        } else {
            std::cout << "PASS Test 5 (confirmed FIPS 202 SHA3-256, not original Keccak-256)\n";
        }
    }

    // Summary
    std::cout << "\n=== SHA3-256 GATE: " << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED") << " ===\n";
    return failures == 0 ? 0 : 1;
}
