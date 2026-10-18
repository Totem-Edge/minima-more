#pragma once

#include <memory>
#include <string>
#include <cstdint>
#include <vector>

#include "org/minima/utils/messages/message_processor.hpp"

// Forward declarations (Rule 10)
namespace org { namespace minima { namespace objects {
class TxPoW;
class TxBlock;
class IBD;
} } }
namespace org { namespace minima { namespace objects { namespace base {
class MiniNumber;
} } } }

namespace org {
namespace minima {
namespace system {
namespace brains {

class TxPoWProcessor : public org::minima::utils::messages::MessageProcessor {
public:
    // Message type constants (mirroring Java private static finals)
    static constexpr const char* TXPOWPROCESSOR_PROCESSTXPOW       = "TXP_PROCESSTXPOW";
    static constexpr const char* TXPOWPROCESSOR_PROCESSTXBLOCK     = "TXP_PROCESSTXBLOCK";
    static constexpr const char* TXPOWPROCESSOR_PROCESS_IBD        = "TXP_PROCESS_IBD";
    static constexpr const char* TXPOWPROCESSOR_PROCESS_SYNCIBD    = "TXP_PROCESS_SYNCIBD";
    static constexpr const char* TXPOWPROCESSOR_PROCESS_ARCHIVEIBD = "TXP_PROCESS_ARCHIVEIBD";
    static constexpr const char* TXPOWPROCESSOR_CHECKER_CALL       = "TXPOWPROCESSOR_CHECKER_CALL";

    TxPoWProcessor();

    // Timers
    void resetFirstIBDTimer();

    // Entry points (post work to message queue)
    void postProcessTxPoW(const std::shared_ptr<org::minima::objects::TxPoW>& zTxPoW);
    void postProcessTxBlock(const std::shared_ptr<org::minima::objects::TxBlock>& zTxBlock);

    void postProcessIBD(const std::shared_ptr<org::minima::objects::IBD>& zIBD, const std::string& zClientUID);
    void postProcessIBD(const std::shared_ptr<org::minima::objects::IBD>& zIBD, const std::string& zClientUID, bool zOverrideRestore);

    bool isIBDProcessFinished();

    void postProcessSyncIBD(const std::shared_ptr<org::minima::objects::IBD>& zIBD, const std::string& zClientUID);
    void postProcessArchiveIBD(const std::shared_ptr<org::minima::objects::IBD>& zIBD, const std::string& zClientUID);

    void onStartUpRecalc();

    void postCheckCall();

protected:
    void processMessage(org::minima::utils::messages::Message& zMessage) override;

private:
    // Internal helpers (mirror Java private methods)
    void processTxPoW(const std::shared_ptr<org::minima::objects::TxPoW>& zTxPoW);
    void processTxBlock(const std::shared_ptr<org::minima::objects::TxBlock>& zTxBlock);
    bool processSyncBlock(const std::shared_ptr<org::minima::objects::TxBlock>& zTxBlock);
    void recalculateTree();

    void askToSyncTxBlocks(const std::string& zClientID);
    void requestMissingTxns(const std::string& zClientID, const std::shared_ptr<org::minima::objects::TxBlock>& zBlock);

private:
    // Constants
    static const org::minima::objects::base::MiniNumber THREE_HOURS;

    // The IBD you receive on startup
    std::int64_t mFirstIBD {0};
    std::int64_t MAX_FIRST_IBD_TIME { static_cast<std::int64_t>(1000) * 60 * 5 };

    // When processing IBD for MegaMMR check if finished..
    bool mIBDSyncFinished { false };
};

} // namespace brains
} // namespace system
} // namespace minima
} // namespace org