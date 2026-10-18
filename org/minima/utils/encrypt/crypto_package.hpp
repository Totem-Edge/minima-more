#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <istream>
#include <ostream>
#include <stdexcept>

#include "org/minima/utils/streamable.hpp"

// Namespaced forward declaration to prevent incomplete-type issues in headers
namespace org { namespace minima { namespace objects { namespace base {
class MiniData;
}}}}

namespace org {
namespace minima {
namespace utils {
namespace encrypt {

class CryptoPackage : public org::minima::utils::Streamable {
public:
    CryptoPackage() = default;
    virtual ~CryptoPackage();                  // For unique_ptr to forward-declared type (PIMPL fix)
    CryptoPackage(CryptoPackage&&) noexcept;   // Move operations declared (PIMPL fix)
    CryptoPackage& operator=(CryptoPackage&&) noexcept;

    // Delete copy operations to respect unique_ptr semantics
    CryptoPackage(const CryptoPackage&) = delete;
    CryptoPackage& operator=(const CryptoPackage&) = delete;

    // Encrypt data by creating an AES secret, encrypting data symmetrically,
    // and encrypting the secret asymmetrically with the given RSA public key.
    // Throws std::exception on errors (maps Java's Exception).
    void encrypt(const std::vector<std::uint8_t>& zData,
                 const std::vector<std::uint8_t>& zRSAPublicKey);

    // Decrypts and returns the plaintext data using the given RSA private key.
    // Throws std::exception on errors (maps Java's Exception).
    std::vector<std::uint8_t> decrypt(const std::vector<std::uint8_t>& zRSAPrivateKey) const;

    // Get the MiniData version of this object (serialized form).
    // Returns nullptr on error (consistent with MiniData::getMiniDataVersion contract).
    std::unique_ptr<org::minima::objects::base::MiniData> getCompleteEncryptedData();

    // Convert a MiniData Version (deserialize from a MiniData blob)
    void ConvertMiniDataVersion(const org::minima::objects::base::MiniData& zComplete);

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Static helper to read from a stream and return a CryptoPackage
    static CryptoPackage ReadFromStream(std::istream& in);

private:
    // Members mirror Java fields; nullptr indicates "unset" like Java's null.
    std::unique_ptr<org::minima::objects::base::MiniData> mIvParam;
    std::unique_ptr<org::minima::objects::base::MiniData> mSecret;
    std::unique_ptr<org::minima::objects::base::MiniData> mData;
};

} // namespace encrypt
} // namespace utils
} // namespace minima
} // namespace org