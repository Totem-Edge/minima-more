#include "org/minima/system/genesis/genesis_m_m_r.hpp"

#include "org/minima/system/genesis/genesis_coin.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/coin.hpp" 
#include "org/minima/objects/mmr/m_m_r_entry.hpp" // <--- ADDED THIS INCLUDE
#include <memory>                      

namespace org {
namespace minima {
namespace system {
namespace genesis {

GenesisMMR::GenesisMMR() : org::minima::objects::mmr::MMR() {
    // This is before the big bang..
    setBlockTime(org::minima::objects::base::MiniNumber::ZERO());

    // Add 1 entry.. the genesis coin..
    // We match the Java logic:
    // 1. Use the base class 'Coin' as the type.
    // 2. Heap-allocate 'GenesisCoin' using a smart pointer.
    std::shared_ptr<org::minima::objects::Coin> gencoin = 
        std::make_shared<GenesisCoin>();

    // Create the MMRData
    // We dereference the pointer (*) to pass the object,
    // and use the -> operator to call the base class method.
    auto mmrdata = org::minima::objects::mmr::MMRData::CreateMMRDataLeafNode(
        *gencoin, gencoin->getAmount());

    // And add to the MMR
    addEntry(*mmrdata);

    // It's done..
    finalizeSet();
}

GenesisMMR::~GenesisMMR() = default;
GenesisMMR::GenesisMMR(GenesisMMR&&) noexcept = default;
GenesisMMR& GenesisMMR::operator=(GenesisMMR&&) noexcept = default;

} // namespace genesis
} // namespace system
} // namespace minima
} // namespace org
