#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <vector>
#include <cstdint>

// Forward declarations (namespaced per Pitfall 10)
namespace org { namespace minima { namespace objects { namespace base { class MiniByte; class MiniData; class MiniNumber; class MiniString; } } } }
namespace org { namespace minima { namespace objects { class Greeting; class IBD; class Pulse; class TxPoW; class TxBlock; } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; class JSONArray; } } } }
namespace org { namespace minima { namespace utils { namespace messages { class Message; class TimerMessage; } } } }
namespace org { namespace minima { namespace system { class Main; } } }
namespace org { namespace minima { namespace database { class MinimaDB; } } }
namespace org { namespace minima { namespace system { namespace network { namespace minima {
class NIOManager;
class NIOClient;
class NIOTraffic;
} } } } }
namespace org { namespace minima { namespace system { namespace network { namespace p2p { class P2PManager; } } } } }
namespace org { namespace minima { namespace system { namespace network { namespace p2p { namespace messages { struct InetSocketAddress; } } } } } }

namespace org {
namespace minima {
namespace system {
namespace network {
namespace minima {

class NIOMessage {
public:
    // Static message type constants (MiniByte)
    static const org::minima::objects::base::MiniByte& MSG_GREETING();
    static const org::minima::objects::base::MiniByte& MSG_IBD();
    static const org::minima::objects::base::MiniByte& MSG_TXPOWID();
    static const org::minima::objects::base::MiniByte& MSG_TXPOWREQ();
    static const org::minima::objects::base::MiniByte& MSG_TXPOW();
    static const org::minima::objects::base::MiniByte& MSG_GENMESSAGE();
    static const org::minima::objects::base::MiniByte& MSG_PULSE();
    static const org::minima::objects::base::MiniByte& MSG_P2P();
    static const org::minima::objects::base::MiniByte& MSG_PING();
    // Message-type slots 9 and 10 are reserved/unused. Do not reuse these values.

    static const org::minima::objects::base::MiniByte& MSG_SINGLE_PING();
    static const org::minima::objects::base::MiniByte& MSG_SINGLE_PONG();

    static const org::minima::objects::base::MiniByte& MSG_IBD_REQ();
    static const org::minima::objects::base::MiniByte& MSG_IBD_RESP();

    static const org::minima::objects::base::MiniByte& MSG_ARCHIVE_REQ();
    static const org::minima::objects::base::MiniByte& MSG_ARCHIVE_DATA();
    static const org::minima::objects::base::MiniByte& MSG_ARCHIVE_SINGLE_REQ();

    static const org::minima::objects::base::MiniByte& MSG_TXBLOCKID();
    static const org::minima::objects::base::MiniByte& MSG_TXBLOCKREQ();
    static const org::minima::objects::base::MiniByte& MSG_TXBLOCK();
    static const org::minima::objects::base::MiniByte& MSG_TXBLOCKMINE();

    static const org::minima::objects::base::MiniByte& MSG_MEGAMMRSYNC_REQ();
    static const org::minima::objects::base::MiniByte& MSG_MEGAMMRSYNC_RESP();

    // Helper convertor
    static std::string convertMessageType(const org::minima::objects::base::MiniByte& zType);

    // Static maps/sets for throttling and tracking
    static std::unordered_map<std::string, org::minima::objects::base::MiniNumber> mlastSyncReq;
    static std::unordered_map<std::string, long long> mLastChainSync;
    static std::unordered_set<std::string> mHaveSentIBDRecently;

    // Static mutexes for thread safety of the above
    static std::mutex s_lastSyncReqMutex;
    static std::mutex s_lastChainSyncMutex;
    static std::mutex s_haveSentIBDMutex;

    // Pending advertisements seen while in IBD (to optionally request after sync)
    static std::vector<org::minima::objects::base::MiniData> mPendingTxPowIDsDuringIBD;
    static std::vector<org::minima::objects::base::MiniData> mPendingTxBlockIDsDuringIBD;
    static std::mutex s_pendingIBDMutex;

    // Drain queued IDs after IBD finishes (requests a bounded number from the given client)
    static void drainPendingDuringIBD(const std::string& zClientUID);

    // Last mine message time
    static long long LAST_TXBLOCKMINE_MSG;

public:
    NIOMessage(const std::string& zClientUID, const org::minima::objects::base::MiniData& zData);

    // PIMPL-FIX for unique_ptr to forward-declared MiniData
    virtual ~NIOMessage();
    NIOMessage(NIOMessage&&) noexcept;
    NIOMessage& operator=(NIOMessage&&) noexcept;

    // Delete copy
    NIOMessage(const NIOMessage&) = delete;
    NIOMessage& operator=(const NIOMessage&) = delete;

    void setFullAddress(const std::string& zAddress);
    void setTrace(bool zTrace, const std::string& zFilter);

    // Main entry
    void run();

private:
    std::string mClientUID;
    std::unique_ptr<org::minima::objects::base::MiniData> mData;

    bool mTrace {false};
    std::string mFilter;

public:
    std::string mFullAdrress;
    int HEAVIER_CHAIN_FOUND {0};
};

} // namespace minima
} // namespace network
} // namespace system
} // namespace minima
} // namespace org