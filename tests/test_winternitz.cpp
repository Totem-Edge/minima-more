// Standalone Winternitz OTS self-test
// Tests the Winternitz algorithm internals without project dependencies.
// Uses OpenSSL for SHA3-256 to match BouncyCastle SHA3Digest(256).

#include <cstdio>
#include <cstring>
#include <vector>
#include <array>
#include <algorithm>
#include <openssl/evp.h>

using Hash32 = std::array<uint8_t, 32>;

static Hash32 sha3_256(const uint8_t* data, size_t len) {
    Hash32 out{};
    unsigned int outlen = 32;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha3_256(), nullptr);
    EVP_DigestUpdate(ctx, data, len);
    EVP_DigestFinal_ex(ctx, out.data(), &outlen);
    EVP_MD_CTX_free(ctx);
    return out;
}

static void print_hash(const char* label, const Hash32& h) {
    printf("%s: ", label);
    for (auto b : h) printf("%02x", b);
    printf("\n");
}

// Winternitz parameters matching Java: w=8, n=32, SHA3-256
static constexpr int W = 8;
static constexpr int N = 32;
static constexpr int L1 = (N * 8 + W - 1) / W;  // 32
static constexpr int MAX_VAL = (1 << W) - 1;     // 255

static int getLog(int n) {
    int n2 = 1, n3 = 2;
    while (n3 < n) { n3 <<= 1; ++n2; }
    return n2;
}

static int L2() { return (getLog((L1 << W) + 1) + W - 1) / W; }
static int L_TOTAL() { return L1 + L2(); }

int main() {
    int passed = 0, total = 0;

    printf("Winternitz parameters: W=%d, N=%d, L1=%d, L2=%d, L_TOTAL=%d\n",
           W, N, L1, L2(), L_TOTAL());

    // Test 1: SHA3-256 basic test (known-answer)
    {
        total++;
        const uint8_t input[] = "abc";
        Hash32 h = sha3_256(input, 3);
        const uint8_t expected[] = {
            0x3a,0x98,0x5d,0xa7,0x4f,0xe2,0x25,0xb2,
            0x04,0x5c,0x17,0x2d,0x6b,0xd3,0x90,0xbd,
            0x85,0x5f,0x08,0x6e,0x3e,0x9d,0x52,0x5b,
            0x46,0xbf,0xe2,0x45,0x11,0x43,0x15,0x32
        };
        bool ok = (memcmp(h.data(), expected, 32) == 0);
        printf("Test 1 (SHA3-256 KAT): %s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++;
    }

    // Test 2: Base-w conversion and checksum roundtrip
    {
        total++;
        uint8_t hash_input[32];
        memset(hash_input, 0, 32);
        hash_input[0] = 0x12;
        hash_input[1] = 0x34;
        
        Hash32 hashed = sha3_256(hash_input, 32);
        
        // Base-w: for w=8, each byte is a digit
        int sum = 0;
        for (int i = 0; i < L1; ++i) {
            sum += hashed[i] & 0xFF;
        }
        
        int checksum = (L1 << W) - sum;
        
        // Convert checksum to base-w (little-endian)
        std::vector<int> csum_digits(L2());
        int cs = checksum;
        for (int i = 0; i < L2(); ++i) {
            csum_digits[i] = cs & MAX_VAL;
            cs >>= W;
        }
        
        // Verify: recompute checksum from base-w digits
        int reconstructed_checksum = 0;
        for (int i = L2() - 1; i >= 0; --i) {
            reconstructed_checksum = (reconstructed_checksum << W) | csum_digits[i];
        }
        
        bool ok = (reconstructed_checksum == checksum);
        printf("Test 2 (base-w conversion): %s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++;
    }

    // Test 3: Key derivation (hash-then-increment) and chain hashing
    {
        total++;
        std::vector<uint8_t> seed(N, 0x42);
        Hash32 current{};
        memcpy(current.data(), seed.data(), N);
        
        // Generate first chain key: hash then increment (GMSSRandom.nextSeed())
        Hash32 chain = sha3_256(current.data(), N);
        // Increment (big-endian)
        for (int i = N - 1; i >= 0; --i) {
            current[i]++;
            if (current[i] != 0) break;
        }
        
        // Hash chain MAX_VAL times to get public key endpoint
        Hash32 pub = chain;
        for (int i = 0; i < MAX_VAL; ++i) {
            pub = sha3_256(pub.data(), N);
        }
        
        // Sign at half, then verify by hashing remaining
        Hash32 sig = chain;
        int half = MAX_VAL / 2;
        for (int i = 0; i < half; ++i) {
            sig = sha3_256(sig.data(), N);
        }
        int remaining = MAX_VAL - half;
        Hash32 derived = sig;
        for (int i = 0; i < remaining; ++i) {
            derived = sha3_256(derived.data(), N);
        }
        
        bool ok = (derived == pub);
        printf("Test 3 (chain hash): %s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++;
    }

    // Test 4: Full sign/verify roundtrip
    {
        total++;
        std::vector<uint8_t> seed(N, 0x42);
        std::vector<uint8_t> msg_data = {0x01, 0x02, 0x03, 0x04, 0x05};
        Hash32 msg_hash = sha3_256(msg_data.data(), msg_data.size());
        
        int l_total = L_TOTAL();
        int l2 = L2();
        
        // Derive private chains (hash-then-increment = GMSSRandom.nextSeed())
        std::vector<Hash32> chains;
        std::vector<uint8_t> cur(N, 0);
        memcpy(cur.data(), seed.data(), N);
        for (int i = 0; i < l_total; ++i) {
            chains.push_back(sha3_256(cur.data(), N));
            for (int j = N - 1; j >= 0; --j) {
                cur[j]++;
                if (cur[j] != 0) break;
            }
        }
        
        // Base-w conversion
        std::vector<int> msg(l_total);
        int sum = 0;
        for (int i = 0; i < L1; ++i) {
            msg[i] = msg_hash[i] & 0xFF;
            sum += msg[i];
        }
        int checksum = (L1 << W) - sum;
        for (int i = 0; i < l2; ++i) {
            msg[L1 + i] = checksum & MAX_VAL;
            checksum >>= W;
        }
        
        // Sign: hash each chain "msg[i]" steps
        std::vector<Hash32> signature;
        for (int i = 0; i < l_total; ++i) {
            Hash32 sig = chains[i];
            for (int j = 0; j < msg[i]; ++j) {
                sig = sha3_256(sig.data(), N);
            }
            signature.push_back(sig);
        }
        
        // Verify: hash remaining steps to get public endpoints
        std::vector<Hash32> endpoints;
        for (int i = 0; i < l_total; ++i) {
            Hash32 ep = signature[i];
            int remaining = MAX_VAL - msg[i];
            for (int j = 0; j < remaining; ++j) {
                ep = sha3_256(ep.data(), N);
            }
            endpoints.push_back(ep);
        }
        
        // Concatenate and hash all endpoints -> public key
        std::vector<uint8_t> concat;
        concat.reserve(l_total * N);
        for (const auto& ep : endpoints) {
            concat.insert(concat.end(), ep.begin(), ep.end());
        }
        Hash32 computed_pubkey = sha3_256(concat.data(), concat.size());
        
        // Compute expected pubkey directly from seed
        std::vector<uint8_t> cur2(N, 0);
        memcpy(cur2.data(), seed.data(), N);
        std::vector<Hash32> chains2;
        for (int i = 0; i < l_total; ++i) {
            chains2.push_back(sha3_256(cur2.data(), N));
            for (int j = N - 1; j >= 0; --j) {
                cur2[j]++;
                if (cur2[j] != 0) break;
            }
        }
        
        std::vector<uint8_t> concat2;
        concat2.reserve(l_total * N);
        for (const auto& ch : chains2) {
            Hash32 ep = ch;
            for (int j = 0; j < MAX_VAL; ++j) {
                ep = sha3_256(ep.data(), N);
            }
            concat2.insert(concat2.end(), ep.begin(), ep.end());
        }
        Hash32 expected_pubkey = sha3_256(concat2.data(), concat2.size());
        
        bool ok = (computed_pubkey == expected_pubkey);
        printf("Test 4 (sign/verify roundtrip): %s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++;
    }

    // Test 5: Wrong message should produce different pubkey
    {
        total++;
        std::vector<uint8_t> seed(N, 0x42);
        std::vector<uint8_t> msg_data = {0x01, 0x02, 0x03, 0x04, 0x05};
        std::vector<uint8_t> wrong_data = {0xFF, 0xFE};
        
        Hash32 msg_hash = sha3_256(msg_data.data(), msg_data.size());
        Hash32 wrong_hash = sha3_256(wrong_data.data(), wrong_data.size());
        
        int l_total = L_TOTAL();
        int l2 = L2();
        
        // Derive chains once
        std::vector<uint8_t> cur(N, 0);
        memcpy(cur.data(), seed.data(), N);
        std::vector<Hash32> chains;
        for (int i = 0; i < l_total; ++i) {
            chains.push_back(sha3_256(cur.data(), N));
            for (int j = N - 1; j >= 0; --j) {
                cur[j]++;
                if (cur[j] != 0) break;
            }
        }
        
        // Sign correct message
        std::vector<int> correct_msg(l_total);
        int sum = 0;
        for (int i = 0; i < L1; ++i) {
            correct_msg[i] = msg_hash[i] & 0xFF;
            sum += correct_msg[i];
        }
        int checksum = (L1 << W) - sum;
        for (int i = 0; i < l2; ++i) {
            correct_msg[L1 + i] = checksum & MAX_VAL;
            checksum >>= W;
        }
        
        std::vector<Hash32> signature;
        for (int i = 0; i < l_total; ++i) {
            Hash32 sig = chains[i];
            for (int j = 0; j < correct_msg[i]; ++j) {
                sig = sha3_256(sig.data(), N);
            }
            signature.push_back(sig);
        }
        
        // Try to verify with WRONG message
        std::vector<Hash32> wrong_endpoints;
        for (int i = 0; i < l_total; ++i) {
            Hash32 ep = signature[i];
            int wrong_val = wrong_hash[i % 32] & 0xFF;
            int remaining = MAX_VAL - wrong_val;
            for (int j = 0; j < remaining; ++j) {
                ep = sha3_256(ep.data(), N);
            }
            wrong_endpoints.push_back(ep);
        }
        
        std::vector<uint8_t> wrong_concat;
        wrong_concat.reserve(l_total * N);
        for (const auto& ep : wrong_endpoints) {
            wrong_concat.insert(wrong_concat.end(), ep.begin(), ep.end());
        }
        Hash32 wrong_pubkey = sha3_256(wrong_concat.data(), wrong_concat.size());
        
        // Get the correct pubkey
        std::vector<uint8_t> cur2(N, 0);
        memcpy(cur2.data(), seed.data(), N);
        std::vector<uint8_t> concat_pk;
        concat_pk.reserve(l_total * N);
        for (int i = 0; i < l_total; ++i) {
            Hash32 ch = sha3_256(cur2.data(), N);
            for (int j = 0; j < MAX_VAL; ++j) {
                ch = sha3_256(ch.data(), N);
            }
            concat_pk.insert(concat_pk.end(), ch.begin(), ch.end());
            // Increment seed
            for (int j = N - 1; j >= 0; --j) {
                cur2[j]++;
                if (cur2[j] != 0) break;
            }
        }
        Hash32 correct_pubkey = sha3_256(concat_pk.data(), concat_pk.size());
        
        bool ok = (wrong_pubkey != correct_pubkey);
        printf("Test 5 (wrong msg -> different pubkey): %s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++;
    }

    printf("\n%d/%d tests passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
