#pragma once

#include <cstddef>
#include <iterator>
#include <random>
#include <string>

#include "org/minima/system/network/p2p/p2_p_functions.hpp"

namespace org { namespace minima { namespace system { namespace network { namespace minima {
class NIOClientInfo;
} } } } }

namespace org { namespace minima { namespace system { namespace network { namespace p2p {

class UtilFuncs {
private:
    UtilFuncs() = delete;

    // Thread-local RNG similar in spirit to a static Random in Java.
    static std::mt19937& rng() {
        static thread_local std::mt19937 gen{ std::random_device{}() };
        return gen;
    }

public:
    // Search a links map for a given address and return the corresponding NIOClientInfo.
    // Returns a unique_ptr so the copied info stays alive for the caller. The previous
    // implementation returned .get() on a temporary unique_ptr, which was a
    // use-after-free bug.
    // Requirements:
    //  - LinksMap must be iterable yielding pairs (key, value)
    //  - key must be convertible to std::string (or std::string itself)
    //  - value must be comparable to Address via operator==
    template <typename Address, typename LinksMap>
    static std::unique_ptr<org::minima::system::network::minima::NIOClientInfo>
    searchLinksMapForAddress(const Address& address, const LinksMap& links) {
        for (const auto& kv : links) {
            const auto& key   = kv.first;
            const auto& value = kv.second;
            if (value == address) {
                return org::minima::system::network::p2p::P2PFunctions::getNIOCLientInfo(static_cast<const std::string&>(key));
            }
        }
        return nullptr;
    }

    // Find client info from an address by searching across state link maps in specific order.
    // Requirements on State:
    //  - state.getInLinks(), getOutLinks(), getNotAcceptingConnP2PLinks(), getNoneP2PLinks()
    //  - Each returns a LinksMap type acceptable to searchLinksMapForAddress
    template <typename Address, typename State>
    static std::unique_ptr<org::minima::system::network::minima::NIOClientInfo>
    getClientFromInetAddress(const Address& address, const State& state) {
        using org::minima::system::network::minima::NIOClientInfo;

        auto clientInfo = searchLinksMapForAddress(address, state.getInLinks());
        if (!clientInfo) {
            clientInfo = searchLinksMapForAddress(address, state.getOutLinks());
        }
        if (!clientInfo) {
            clientInfo = searchLinksMapForAddress(address, state.getNotAcceptingConnP2PLinks());
        }
        if (!clientInfo) {
            clientInfo = searchLinksMapForAddress(address, state.getNoneP2PLinks());
        }
        return clientInfo;
    }

    // Select a random address from a container of addresses.
    // Returns pointer to the selected element, or nullptr if the container is empty.
    // Works with any container supporting size(), begin(), and forward iteration.
    template <typename AddressContainer>
    static const typename AddressContainer::value_type*
    selectRandomAddress(const AddressContainer& addresses) {
        using Value = typename AddressContainer::value_type;

        const Value* returnAddress = nullptr;

        const auto sz = addresses.size();
        if (sz > 0) {
            std::size_t idx = 0;
            if (sz > 1) {
                std::uniform_int_distribution<std::size_t> dist(0, sz - 1);
                idx = dist(rng());
            }
            auto it = addresses.begin();
            std::advance(it, static_cast<std::ptrdiff_t>(idx));
            returnAddress = std::addressof(*it);
        }
        return returnAddress;
    }
};

} } } } }