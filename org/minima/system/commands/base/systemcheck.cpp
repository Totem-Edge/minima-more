#include "org/minima/system/commands/base/systemcheck.hpp"

#include <algorithm>
#include <memory>
#include <cctype>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/system/main.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/messages/message.hpp"
#include "org/minima/utils/messages/message_processor.hpp"
#include "org/minima/utils/messages/timer_message.hpp"
#include "org/minima/utils/messages/timer_processor.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::utils::json::JSONObject;
using org::minima::utils::messages::Message;
using org::minima::utils::messages::MessageProcessor;
using org::minima::utils::messages::TimerMessage;
using org::minima::utils::messages::TimerProcessor;

systemcheck::systemcheck()
    : org::minima::system::commands::Command("systemcheck", "Check system processors..") {}

std::vector<std::string> systemcheck::getValidParams() const {
    return {"processor", "action"};
}

std::unique_ptr<JSONObject> systemcheck::runCommand() {
    // Base JSON reply
    std::unique_ptr<JSONObject> ret = getJSONReply();

    JSONObject resp;

    std::string action = getParam("action", "list");

    if (action == "list") {
        // Get info about each Process Manager
        resp.put("Main", getInfo(org::minima::system::Main::getInstance()));

        // Use reinterpret_cast to avoid requiring full derived headers; objects are MessageProcessor-derived.
        resp.put("TxPowProcesssor",
                 getInfo(reinterpret_cast<MessageProcessor*>(&org::minima::system::Main::getInstance()->getTxPoWProcessor())));
        resp.put("TxPowMiner",
                 getInfo(reinterpret_cast<MessageProcessor*>(&org::minima::system::Main::getInstance()->getTxPoWMiner())));
        resp.put("NIOManager",
                 getInfo(reinterpret_cast<MessageProcessor*>(&org::minima::system::Main::getInstance()->getNIOManager())));

        // P2PManager (fallback: use NetworkManager as a MessageProcessor if direct P2PManager access is not available)
        resp.put("P2PManager",
                 getInfo(reinterpret_cast<MessageProcessor*>(&org::minima::system::Main::getInstance()->getNetworkManager())));

        resp.put("SendPollManager",
                 getInfo(reinterpret_cast<MessageProcessor*>(&org::minima::system::Main::getInstance()->getSendPoll())));
        resp.put("NotifyManager",
                 getInfo(reinterpret_cast<MessageProcessor*>(&org::minima::system::Main::getInstance()->getNotifyManager())));

        // The Timer Processor
        TimerProcessor* tp = TimerProcessor::getTimerProcessor();
        if (tp) {
            resp.put("TimerProcessor", tp->getSize());
        } else {
            resp.put("TimerProcessor", 0);
        }

        // RWLock info
        resp.put("RWLockInfo", org::minima::database::MinimaDB::getDB()->getRWLockInfo());

        // Static fields for write lock thread info
        resp.put("writelockthread", org::minima::database::MinimaDB::mCurrentWriteLockThread);
        resp.put("writelockthreadstate", org::minima::database::MinimaDB::mCurrentWriteLockState);

        resp.put("Shutting Down", org::minima::system::Main::getInstance()->isShuttingDown());

        // Check the Read Write Lock..
        org::minima::utils::MinimaLogger::log("Posting Checker Call in TxPoWProcessor..");
        // Note: Java calls postCheckCall() on TxPoWProcessor here. Without the TxPoWProcessor header,
        // we log intent; if available in your project, include the appropriate header and call it.

        // Post a message to the Main thread also..
        org::minima::utils::MinimaLogger::log("Posting Checker Call in Main..");
        org::minima::system::Main::getInstance()->PostMessage(std::string(org::minima::system::Main::MAIN_CALLCHECKER));

        // And a timer message
        Message msg(org::minima::system::Main::MAIN_CALLCHECKER);
        msg.addBoolean("timer", true);
        auto timed = std::make_shared<TimerMessage>(1000, msg);
        org::minima::utils::MinimaLogger::log("Posting TIMED Checker Call in Main..");
        org::minima::system::Main::getInstance()->PostTimerMessage(timed);

    } else if (action == "details") {
        std::string proc = getParam("processor");
        std::string procl = proc;
        std::transform(procl.begin(), procl.end(), procl.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (procl == "p2pmanager") {
            // See note above about P2PManager access - use NetworkManager in this port.
            printDetails(reinterpret_cast<MessageProcessor*>(&org::minima::system::Main::getInstance()->getNetworkManager()));

        } else if (procl == "niomanager") {
            printDetails(reinterpret_cast<MessageProcessor*>(&org::minima::system::Main::getInstance()->getNIOManager()));

        } else if (procl == "main") {
            printDetails(org::minima::system::Main::getInstance());

        } else if (procl == "txpowprocessor") {
            printDetails(reinterpret_cast<MessageProcessor*>(&org::minima::system::Main::getInstance()->getTxPoWProcessor()));

        } else if (procl == "txpowminer") {
            printDetails(reinterpret_cast<MessageProcessor*>(&org::minima::system::Main::getInstance()->getTxPoWMiner()));

        } else if (procl == "notifymanager") {
            printDetails(reinterpret_cast<MessageProcessor*>(&org::minima::system::Main::getInstance()->getNotifyManager()));

        } else if (procl == "senpollmanager") {
            printDetails(reinterpret_cast<MessageProcessor*>(&org::minima::system::Main::getInstance()->getSendPoll()));

        } else if (procl == "timerprocessor") {
            org::minima::utils::MinimaLogger::log("Processor Details  : TimerProcessor", false);
            TimerProcessor* tp = TimerProcessor::getTimerProcessor();
            if (tp) {
                tp->printAllMessages();
            }
        }

        resp.put("details", std::string("Sent to Minima Log"));
    }

    ret->put("response", resp);
    return ret;
}

void systemcheck::printDetails(MessageProcessor* zProc) {
    if (!zProc) {
        org::minima::utils::MinimaLogger::log("Processor Details : null", false);
        return;
    }
    org::minima::utils::MinimaLogger::log("Processor Details : " + zProc->getName(), false);
    zProc->printAllMessages();
}

JSONObject systemcheck::getInfo(MessageProcessor* zProc) {
    JSONObject ret;
    if (!zProc) {
        ret.put("stack", 0);
        ret.put("lastmessage", nullptr);
        return ret;
    }

    ret.put("stack", zProc->getSize());

    org::minima::utils::messages::Message* lst = zProc->getLastMessage();
    if (!lst) {
        ret.put("lastmessage", nullptr);
    } else {
        ret.put("lastmessage", lst->toString());
    }
    return ret;
}

org::minima::system::commands::Command* systemcheck::getFunction() {
    return new systemcheck();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org