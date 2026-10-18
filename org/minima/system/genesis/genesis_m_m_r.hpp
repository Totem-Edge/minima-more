#pragma once

#include "org/minima/objects/mmr/m_m_r.hpp"

namespace org {
namespace minima {
namespace system {
namespace genesis {

class GenesisMMR : public org::minima::objects::mmr::MMR {
public:
    GenesisMMR();
    virtual ~GenesisMMR();
    GenesisMMR(GenesisMMR&&) noexcept;
    GenesisMMR& operator=(GenesisMMR&&) noexcept;

    GenesisMMR(const GenesisMMR&) = delete;
    GenesisMMR& operator=(const GenesisMMR&) = delete;
};

} // namespace genesis
} // namespace system
} // namespace minima
} // namespace org