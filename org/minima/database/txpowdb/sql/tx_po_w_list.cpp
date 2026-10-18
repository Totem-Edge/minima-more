#include "org/minima/database/txpowdb/sql/tx_po_w_list.hpp"

// Full headers for used types
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/minima_logger.hpp"

#include <sstream>
#include <stdexcept>
#include <iostream>

//
// ### FIX 1 ###
// Replaced <arpa/inet.h> with the Windows equivalent for network functions
//
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <winsock2.h>
#endif


// Helper functions for binary I/O with std::streams
// (These replace DataOutputStream.writeInt / DataInputStream.readInt)
namespace {
void writeInt(std::ostream& zOut, int32_t val) {
    // Assuming network byte order (big-endian) for consistency with Java
    val = htonl(val);
    zOut.write(reinterpret_cast<const char*>(&val), sizeof(int32_t));
}

int32_t readInt(std::istream& zIn) {
    int32_t val = 0;
    zIn.read(reinterpret_cast<char*>(&val), sizeof(int32_t));
    if (zIn.gcount() != sizeof(int32_t)) {
        throw std::runtime_error("readInt: Failed to read 4 bytes");
    }
    // Convert from network byte order
    return ntohl(val);
}
} // namespace

namespace org { namespace minima { namespace txpowdb { namespace sql {

TxPoWList::TxPoWList() : mTxPoWs() {}

TxPoWList::TxPoWList(std::vector<std::unique_ptr<org::minima::objects::TxPoW>> zTxPoWs)
    : mTxPoWs(std::move(zTxPoWs)) {}

TxPoWList::~TxPoWList() = default;
TxPoWList::TxPoWList(TxPoWList&&) noexcept = default;
TxPoWList& TxPoWList::operator=(TxPoWList&&) noexcept = default;

void TxPoWList::writeDataStream(std::ostream& zOut) {
    const int32_t sz = static_cast<int32_t>(mTxPoWs.size());
    ::writeInt(zOut, sz); // Use helper
    for (const auto& txp : mTxPoWs) {
        if (!txp) {
            throw std::runtime_error("TxPoWList::writeDataStream encountered null TxPoW entry");
        }
        txp->writeDataStream(zOut); 
    }
}

void TxPoWList::readDataStream(std::istream& zIn) {
    mTxPoWs.clear();
    const int32_t size = ::readInt(zIn); // Use helper
    if (size < 0) {
        throw std::runtime_error("TxPoWList::readDataStream negative list size");
    }
    mTxPoWs.reserve(static_cast<size_t>(size));
    for (int i = 0; i < size; ++i) {
        //
        // ### FIX 2 (Already applied from previous answer) ###
        // TxPoW::ReadFromStream returns a TxPoW object by value.
        // We must create a new heap-allocated copy for our unique_ptr.
        //
        std::unique_ptr<org::minima::objects::TxPoW> txp = 
            org::minima::objects::TxPoW::ReadFromStream(zIn);
        
        if (!txp) {
            throw std::runtime_error("TxPoWList::readDataStream failed to read TxPoW");
        }
        mTxPoWs.emplace_back(std::move(txp));
    }
}

std::unique_ptr<TxPoWList> TxPoWList::ReadFromStream(std::istream& zIn) {
    auto txplist = std::unique_ptr<TxPoWList>(new TxPoWList());
    txplist->readDataStream(zIn);
    return txplist;
}

std::unique_ptr<TxPoWList> TxPoWList::convertMiniDataVersion(const org::minima::objects::base::MiniData& zTxpData) {
    std::unique_ptr<TxPoWList> txnrow;
    try {
        // Create a stringstream over the raw bytes
        const auto& bytes = zTxpData.getBytes();
        std::string dataStr(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        std::stringstream dis(dataStr, std::ios::in | std::ios::binary);

        txnrow = TxPoWList::ReadFromStream(dis);
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e.what());
        txnrow.reset();
    }
    return txnrow;
}

} } } } // end namespace org::minima::txpowdb::sql