#include "org/minima/utils/encrypt/crypto_package.hpp"

#include <sstream>
#include <utility>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/encrypt/generate_key.hpp"
#include "org/minima/utils/encrypt/encrypt_decrypt.hpp"

namespace org {
namespace minima {
namespace utils {
namespace encrypt {

// Define special members for unique_ptr to forward-declared types (PIMPL fix)
CryptoPackage::~CryptoPackage() = default;
CryptoPackage::CryptoPackage(CryptoPackage&&) noexcept = default;
CryptoPackage& CryptoPackage::operator=(CryptoPackage&&) noexcept = default;

void CryptoPackage::encrypt(const std::vector<std::uint8_t>& zData,
                            const std::vector<std::uint8_t>& zRSAPublicKey) {
    // Create an IvParam for this round of encryption
    std::vector<std::uint8_t> ivparam = GenerateKey::IvParam();
    mIvParam = std::make_unique<org::minima::objects::base::MiniData>(ivparam);

    // Create an AES key (symmetric secret)
    std::vector<std::uint8_t> secret = GenerateKey::secretKey();

    // Encrypt the data with the secret
    std::vector<std::uint8_t> encrypteddata = EncryptDecrypt::encryptSYM(ivparam, secret, zData);
    mData = std::make_unique<org::minima::objects::base::MiniData>(encrypteddata);

    // Encrypt the secret with the RSA Public Key
    std::vector<std::uint8_t> encryptedsecret = EncryptDecrypt::encryptASM(zRSAPublicKey, secret);
    mSecret = std::make_unique<org::minima::objects::base::MiniData>(encryptedsecret);
}

std::vector<std::uint8_t> CryptoPackage::decrypt(const std::vector<std::uint8_t>& zRSAPrivateKey) const {
    if (!mSecret || !mIvParam || !mData) {
        throw std::runtime_error("CryptoPackage: decrypt called on uninitialized package");
    }

    // First decrypt the secret
    std::vector<std::uint8_t> secret = EncryptDecrypt::decryptASM(zRSAPrivateKey, mSecret->getBytes());

    // Now decrypt the data
    std::vector<std::uint8_t> dec = EncryptDecrypt::decryptSYM(mIvParam->getBytes(), secret, mData->getBytes());

    return dec;
}

std::unique_ptr<org::minima::objects::base::MiniData> CryptoPackage::getCompleteEncryptedData() {
    // Serialize this object to MiniData
    return org::minima::objects::base::MiniData::getMiniDataVersion(*this);
}

void CryptoPackage::ConvertMiniDataVersion(const org::minima::objects::base::MiniData& zComplete) {
    const auto& bytes = zComplete.getBytes();
    std::string buffer(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::istringstream in(buffer, std::ios::binary);
    readDataStream(in);
    if (!in) {
        throw std::ios_base::failure("CryptoPackage: failed to read from MiniData buffer");
    }
}

void CryptoPackage::writeDataStream(std::ostream& out) {
    if (!mIvParam || !mSecret || !mData) {
        throw std::runtime_error("CryptoPackage: writeDataStream called on uninitialized package");
    }
    mIvParam->writeDataStream(out);
    mSecret->writeDataStream(out);
    mData->writeDataStream(out);
}

void CryptoPackage::readDataStream(std::istream& in) {
    org::minima::objects::base::MiniData iv  = org::minima::objects::base::MiniData::ReadFromStream(in);
    org::minima::objects::base::MiniData sec = org::minima::objects::base::MiniData::ReadFromStream(in);
    org::minima::objects::base::MiniData dat = org::minima::objects::base::MiniData::ReadFromStream(in);

    mIvParam = std::make_unique<org::minima::objects::base::MiniData>(iv);
    mSecret  = std::make_unique<org::minima::objects::base::MiniData>(sec);
    mData    = std::make_unique<org::minima::objects::base::MiniData>(dat);
}

CryptoPackage CryptoPackage::ReadFromStream(std::istream& in) {
    CryptoPackage crypt;
    crypt.readDataStream(in);
    return crypt;
}

} // namespace encrypt
} // namespace utils
} // namespace minima
} // namespace org

// Optional demo main: define this macro to build the example/test harness.
// This avoids forcing an entry point into libraries by default.
#ifdef ORG_MINIMA_UTILS_ENCRYPT_CRYPTO_PACKAGE_DEMO_MAIN
#include <iostream>
#include "org/minima/utils/encrypt/generate_key.hpp"
#include "org/minima/objects/base/mini_data.hpp"

int main(int argc, char* argv[]) {
    using org::minima::objects::base::MiniData;
    using org::minima::utils::encrypt::CryptoPackage;

    auto kp = org::minima::utils::encrypt::GenerateKey::generateKeyPair();

    std::vector<uint8_t> publicKey  = kp.publicKeyEncoded;
    MiniData pubk(publicKey);

    std::vector<uint8_t> privateKey = kp.privateKeyEncoded;
    MiniData privk(privateKey);

    MiniData rdata = MiniData::getRandomData(256);
    CryptoPackage cp;
    cp.encrypt(rdata.getBytes(), publicKey);

    std::cout << "Public Key  : " << pubk.getLength() << "\n";
    std::cout << "Private Key : " << privk.getLength() << "\n";
    // Accessors for internals are not provided; this line is illustrative.
    // std::cout << "Secret Key  : " << cp.getSecret().getLength() << "\n";
    std::cout << "Data        : " << rdata.getLength() << "\n";
    auto encdata = cp.getCompleteEncryptedData();
    std::cout << "Enc Data    : " << (encdata ? encdata->getLength() : 0) << "\n";

    std::vector<uint8_t> dec = cp.decrypt(privateKey);
    MiniData decdata(dec);

    std::cout << "Worked     : " << (decdata.isEqual(rdata) ? "true" : "false") << "\n";

    return 0;
}
#endif