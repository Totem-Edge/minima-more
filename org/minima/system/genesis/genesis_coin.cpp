#include "org/minima/system/genesis/genesis_coin.hpp"

#include "org/minima/objects/address.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#ifdef _WIN32
// No Windows-specific code required for this class.
#endif

namespace org {
namespace minima {
namespace system {
namespace genesis {

// Define the static GENESIS_COINID with the exact hex string from Java
const org::minima::objects::base::MiniData GenesisCoin::GENESIS_COINID(
    "0x5350415254414355534C4F5645534D494E494D41"
);

// Constructor: super(GENESIS_COINID, Address.TRUE_ADDRESS.getAddressData(), MiniNumber.BILLION, Token.TOKENID_MINIMA)
GenesisCoin::GenesisCoin()
: org::minima::objects::Coin(
      GenesisCoin::GENESIS_COINID,
      org::minima::objects::Address::getTrueAddress().getAddressData(),
      org::minima::objects::base::MiniNumber::BILLION(),
      org::minima::objects::Token::TOKENID_MINIMA) {
}

} // namespace genesis
} // namespace system
} // namespace minima
} // namespace org