#pragma once

#include <string>

// Base class include (inheritance requires full header per Pitfall 10)
#include "org/minima/objects/tx_po_w.hpp"

namespace org {
namespace minima {
namespace system {
namespace genesis {

class GenesisTxPoW final : public org::minima::objects::TxPoW {
public:
    explicit GenesisTxPoW(const std::string& zGenesisAddress);

    // Special members
    virtual ~GenesisTxPoW();
    GenesisTxPoW(GenesisTxPoW&&) noexcept;
    GenesisTxPoW& operator=(GenesisTxPoW&&) noexcept;

    GenesisTxPoW(const GenesisTxPoW&) = delete;
    GenesisTxPoW& operator=(const GenesisTxPoW&) = delete;
};

} // namespace genesis
} // namespace system
} // namespace minima
} // namespace org