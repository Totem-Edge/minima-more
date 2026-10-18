#include "org/minima/utils/encrypt/password_crypto.hpp"

#include <stdexcept>
#include <vector>
#include <sstream>
#include <iostream>
#include <iterator>
#include <cstring>

#include "org/minima/objects/base/mini_data.hpp"

// OpenSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>

// zlib
#include <zlib.h>

namespace org {
namespace minima {
namespace utils {
namespace encrypt {

namespace {

// Configuration defaults (assumed in absence of GenerateKey.java)
constexpr int KDF_ITERATIONS = 100000;          // PBKDF2 iterations (tunable)
constexpr std::size_t AES_KEY_LEN = 32;         // AES-256
constexpr std::size_t AES_BLOCK_LEN = 16;       // AES block size / IV length
constexpr std::size_t SALT_LEN = 8;             // As used by the Java code

std::string openssl_last_error() {
    unsigned long err = ERR_get_error();
    if (err == 0) return std::string();
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    return std::string(buf);
}

std::vector<std::uint8_t> pbkdf2_hmac_sha256(const std::string& password,
                                             const std::vector<std::uint8_t>& salt,
                                             int iterations,
                                             std::size_t key_len) {
    std::vector<std::uint8_t> key(key_len);
    int ok = PKCS5_PBKDF2_HMAC(
        password.c_str(), static_cast<int>(password.size()),
        salt.empty() ? nullptr : salt.data(), static_cast<int>(salt.size()),
        iterations, EVP_sha256(),
        static_cast<int>(key_len), key.data()
    );
    if (ok != 1) {
        throw std::runtime_error("PBKDF2_HMAC_SHA256 failed: " + openssl_last_error());
    }
    return key;
}

std::vector<std::uint8_t> aes_encrypt_cbc(const std::vector<std::uint8_t>& plaintext,
                                          const std::vector<std::uint8_t>& key,
                                          const std::vector<std::uint8_t>& iv) {
    const EVP_CIPHER* cipher = nullptr;
    switch (key.size()) {
        case 16: cipher = EVP_aes_128_cbc(); break;
        case 24: cipher = EVP_aes_192_cbc(); break;
        case 32: cipher = EVP_aes_256_cbc(); break;
        default: throw std::runtime_error("Unsupported AES key length");
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");

    std::vector<std::uint8_t> ciphertext(plaintext.size() + AES_BLOCK_LEN);
    int len = 0;
    int total = 0;

    if (EVP_EncryptInit_ex(ctx, cipher, nullptr, key.data(), iv.data()) != 1) {
        std::string err = openssl_last_error();
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptInit_ex failed: " + err);
    }

    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(),
                          static_cast<int>(plaintext.size())) != 1) {
        std::string err = openssl_last_error();
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptUpdate failed: " + err);
    }
    total = len;

    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + total, &len) != 1) {
        std::string err = openssl_last_error();
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptFinal_ex failed: " + err);
    }
    total += len;
    ciphertext.resize(static_cast<std::size_t>(total));
    EVP_CIPHER_CTX_free(ctx);
    return ciphertext;
}

std::vector<std::uint8_t> aes_decrypt_cbc(const std::vector<std::uint8_t>& ciphertext,
                                          const std::vector<std::uint8_t>& key,
                                          const std::vector<std::uint8_t>& iv) {
    const EVP_CIPHER* cipher = nullptr;
    switch (key.size()) {
        case 16: cipher = EVP_aes_128_cbc(); break;
        case 24: cipher = EVP_aes_192_cbc(); break;
        case 32: cipher = EVP_aes_256_cbc(); break;
        default: throw std::runtime_error("Unsupported AES key length");
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");

    std::vector<std::uint8_t> plaintext(ciphertext.size() + AES_BLOCK_LEN);
    int len = 0;
    int total = 0;

    if (EVP_DecryptInit_ex(ctx, cipher, nullptr, key.data(), iv.data()) != 1) {
        std::string err = openssl_last_error();
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_DecryptInit_ex failed: " + err);
    }

    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(),
                          static_cast<int>(ciphertext.size())) != 1) {
        std::string err = openssl_last_error();
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_DecryptUpdate failed: " + err);
    }
    total = len;

    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + total, &len) != 1) {
        std::string err = openssl_last_error();
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_DecryptFinal_ex failed: " + err);
    }
    total += len;
    plaintext.resize(static_cast<std::size_t>(total));
    EVP_CIPHER_CTX_free(ctx);
    return plaintext;
}

// GZIP compress using zlib (deflate with gzip header)
std::vector<std::uint8_t> gzip_compress(const std::vector<std::uint8_t>& input) {
    z_stream strm{};
    int ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) throw std::runtime_error("deflateInit2 failed");

    std::vector<std::uint8_t> out;
    out.reserve(input.size() / 2 + 64);

    constexpr std::size_t CHUNK = 16384;
    std::vector<std::uint8_t> outbuf(CHUNK);

    strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(input.data()));
    strm.avail_in = static_cast<uInt>(input.size());

    do {
        strm.next_out = outbuf.data();
        strm.avail_out = static_cast<uInt>(outbuf.size());

        int flush = (strm.avail_in == 0) ? Z_FINISH : Z_NO_FLUSH;
        ret = deflate(&strm, flush);
        if (ret == Z_STREAM_ERROR) {
            deflateEnd(&strm);
            throw std::runtime_error("deflate Z_STREAM_ERROR");
        }

        std::size_t have = outbuf.size() - strm.avail_out;
        out.insert(out.end(), outbuf.data(), outbuf.data() + have);
    } while (ret != Z_STREAM_END);

    deflateEnd(&strm);
    return out;
}

// GZIP decompress using zlib (inflate with gzip header)
std::vector<std::uint8_t> gzip_decompress(const std::vector<std::uint8_t>& input) {
    z_stream strm{};
    int ret = inflateInit2(&strm, 15 + 16);
    if (ret != Z_OK) throw std::runtime_error("inflateInit2 failed");

    std::vector<std::uint8_t> out;
    out.reserve(input.size() * 2 + 64);

    constexpr std::size_t CHUNK = 16384;
    std::vector<std::uint8_t> outbuf(CHUNK);

    strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(input.data()));
    strm.avail_in = static_cast<uInt>(input.size());

    do {
        strm.next_out = outbuf.data();
        strm.avail_out = static_cast<uInt>(outbuf.size());

        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&strm);
            throw std::runtime_error("inflate error");
        }
        std::size_t have = outbuf.size() - strm.avail_out;
        out.insert(out.end(), outbuf.data(), outbuf.data() + have);
    } while (ret != Z_STREAM_END);

    inflateEnd(&strm);
    return out;
}

std::vector<std::uint8_t> random_bytes(std::size_t len) {
    std::vector<std::uint8_t> buf(len);
    if (len > 0) {
        if (RAND_bytes(buf.data(), static_cast<int>(len)) != 1) {
            throw std::runtime_error("RAND_bytes failed: " + openssl_last_error());
        }
    }
    return buf;
}

} // anonymous namespace

org::minima::objects::base::MiniData PasswordCrypto::encryptPassword(
    const std::string& zPassword,
    const org::minima::objects::base::MiniData& zData) {

    using org::minima::objects::base::MiniData;

    // 1) Generate Salt (8 bytes) and IV (16 bytes)
    MiniData salt = MiniData::getRandomData(static_cast<int>(SALT_LEN));
    std::vector<std::uint8_t> iv = random_bytes(AES_BLOCK_LEN);

    // 2) Derive key from password and salt
    std::vector<std::uint8_t> secret = pbkdf2_hmac_sha256(zPassword, salt.getBytes(), KDF_ITERATIONS, AES_KEY_LEN);

    // 3) Serialize payload (MiniData.writeDataStream) - writeDataStream is non-const, so copy first
    MiniData dataCopy = zData;
    std::ostringstream payload_stream(std::ios::binary);
    dataCopy.writeDataStream(payload_stream);
    const std::string payload_str = payload_stream.str();
    std::vector<std::uint8_t> payload(payload_str.begin(), payload_str.end());

    // 4) GZIP compress the payload
    std::vector<std::uint8_t> compressed = gzip_compress(payload);

    // 5) AES encrypt compressed data
    std::vector<std::uint8_t> ciphertext = aes_encrypt_cbc(compressed, secret, iv);

    // 6) Construct final output: salt.writeDataStream, iv.writeDataStream, then ciphertext raw bytes
    MiniData ivmd(iv);
    std::ostringstream out(std::ios::binary);
    salt.writeDataStream(out);
    ivmd.writeDataStream(out);
    out.write(reinterpret_cast<const char*>(ciphertext.data()), static_cast<std::streamsize>(ciphertext.size()));

    const std::string outstr = out.str();
    std::vector<std::uint8_t> finalbytes(outstr.begin(), outstr.end());
    return MiniData(finalbytes);
}

org::minima::objects::base::MiniData PasswordCrypto::decryptPassword(
    const std::string& zPassword,
    const org::minima::objects::base::MiniData& zEncryptedData) {

    using org::minima::objects::base::MiniData;

    // Prepare input stream
    const std::vector<std::uint8_t>& inbytes = zEncryptedData.getBytes();
    std::istringstream in(std::string(reinterpret_cast<const char*>(inbytes.data()), inbytes.size()),
                          std::ios::binary);

    // 1) Read salt and IV MiniData frames
    MiniData salt = MiniData::ReadFromStream(in);
    MiniData ivmd = MiniData::ReadFromStream(in);
    const std::vector<std::uint8_t>& iv = ivmd.getBytes();

    if (iv.size() != AES_BLOCK_LEN) {
        throw std::runtime_error("Unexpected IV length in encrypted data");
    }

    // 2) Read remaining bytes as ciphertext
    std::vector<std::uint8_t> ciphertext;
    {
        std::string rest((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        ciphertext.assign(rest.begin(), rest.end());
    }

    // 3) Derive key
    std::vector<std::uint8_t> secret = pbkdf2_hmac_sha256(zPassword, salt.getBytes(), KDF_ITERATIONS, AES_KEY_LEN);

    // 4) Decrypt
    std::vector<std::uint8_t> decompressed_input = aes_decrypt_cbc(ciphertext, secret, iv);

    // 5) GZIP decompress
    std::vector<std::uint8_t> plain = gzip_decompress(decompressed_input);

    // 6) Read MiniData from plaintext stream
    std::istringstream pstream(std::string(reinterpret_cast<const char*>(plain.data()), plain.size()),
                               std::ios::binary);
    MiniData decrypted = MiniData::ReadFromStream(pstream);

    return decrypted;
}

} // namespace encrypt
} // namespace utils
} // namespace minima
} // namespace org
