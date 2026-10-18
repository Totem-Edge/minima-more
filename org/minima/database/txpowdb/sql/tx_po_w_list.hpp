#pragma once

#include <memory>
#include <vector>
#include <utility>
#include <iostream> // Added for std::ostream/istream

// Base class include (Rule 1: Inheritance)
#include "org/minima/utils/streamable.hpp"

// Namespaced Forward declarations for project classes (Rule 2 & 4)
namespace org { namespace minima { namespace objects { class TxPoW; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; } } } }
// Removed forward declarations for DataInputStream/DataOutputStream as they are replaced
// by <iostream>

// Java Package to C++ Namespace Mapping for TxPoWList: org.minima.objects
namespace org { namespace minima { namespace txpowdb { namespace sql {
} } } } // end namespace org::minima::txpowdb::sql

namespace org { namespace minima { namespace txpowdb { namespace sql {

class TxPoWList : public org::minima::utils::Streamable {
public:
    std::vector<std::unique_ptr<org::minima::objects::TxPoW>> mTxPoWs;

private:
    TxPoWList();

public:
    explicit TxPoWList(std::vector<std::unique_ptr<org::minima::objects::TxPoW>> zTxPoWs);

    ~TxPoWList();
    TxPoWList(TxPoWList&&) noexcept;
    TxPoWList& operator=(TxPoWList&&) noexcept;

    TxPoWList(const TxPoWList&) = delete;
    TxPoWList& operator=(const TxPoWList&) = delete;

    //
    // ### FIX 1 ###
    // Signatures changed to match the Streamable base class.
    //
    void writeDataStream(std::ostream& zOut) override;
    void readDataStream(std::istream& zIn) override;

    //
    // ### FIX 2 ###
    // Static helper signature also updated.
    //
    static std::unique_ptr<TxPoWList> ReadFromStream(std::istream& zIn);

    static std::unique_ptr<TxPoWList> convertMiniDataVersion(const org::minima::objects::base::MiniData& zTxpData);
};

} } } } // end namespace org::minima::txpowdb::sql