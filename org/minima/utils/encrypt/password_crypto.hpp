#pragma once

#include <string>

namespace org {
namespace minima {
namespace objects {
namespace base {
class MiniData;
} // namespace base
} // namespace objects
} // namespace minima
} // namespace org

namespace org {
namespace minima {
namespace utils {
namespace encrypt {

class PasswordCrypto {
public:
    // Encrypt zData using password zPassword:
    // Output format:
    //   MiniData.writeDataStream(salt)
    //   MiniData.writeDataStream(iv)
    //   raw bytes: AES-CBC(encrypt( GZIP( MiniData.writeDataStream(zData) )))
    // Throws std::runtime_error on errors.
    static org::minima::objects::base::MiniData encryptPassword(const std::string& zPassword,
                                                                const org::minima::objects::base::MiniData& zData);

    // Decrypt the data produced by encryptPassword with the same password.
    // Throws std::runtime_error on errors.
    static org::minima::objects::base::MiniData decryptPassword(const std::string& zPassword,
                                                                const org::minima::objects::base::MiniData& zEncryptedData);
};

} // namespace encrypt
} // namespace utils
} // namespace minima
} // namespace org