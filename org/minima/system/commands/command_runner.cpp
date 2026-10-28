#include "org/minima/system/commands/command_runner.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/params/global_params.hpp"
#include "org/minima/system/brains/tx_po_w_checker.hpp"

// Base command support
#include "org/minima/system/commands/command_exception.hpp"

// Logger
#include "org/minima/utils/minima_logger.hpp"

// JSON
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/parser/j_s_o_n_parser.hpp"
#include "org/minima/utils/json/parser/parse_exception.hpp"

// Only include missingcmd for fallback behavior
#include "org/minima/system/commands/base/missingcmd.hpp"

// Commands in the root package (org::minima::system::commands)
#include "org/minima/system/commands/help.hpp"
#include "org/minima/system/commands/tutorial.hpp"
#include "org/minima/system/commands/whitepaper.hpp"

// Backup commands (org::minima::system::commands::backup)
#include "org/minima/system/commands/backup/archive.hpp"
#include "org/minima/system/commands/backup/backup.hpp"
#include "org/minima/system/commands/backup/decryptbackup.hpp"
#include "org/minima/system/commands/backup/mysql.hpp"
#include "org/minima/system/commands/backup/mysqlcoins.hpp"
#include "org/minima/system/commands/backup/reset.hpp"
#include "org/minima/system/commands/backup/restore.hpp"
#include "org/minima/system/commands/backup/restoresync.hpp"
#include "org/minima/system/commands/backup/vault.hpp"

// Backup MMRSync commands (org::minima::system::commands::backup::mmrsync)
#include "org/minima/system/commands/backup/mmrsync/megammr.hpp"
#include "org/minima/system/commands/backup/mmrsync/megammrsync.hpp"

// Base commands (org::minima::system::commands::base)
#include "org/minima/system/commands/base/automine.hpp"
#include "org/minima/system/commands/base/balance.hpp"
#include "org/minima/system/commands/base/block.hpp"
#include "org/minima/system/commands/base/burn.hpp"
#include "org/minima/system/commands/base/checkaddress.hpp"
#include "org/minima/system/commands/base/coincheck.hpp"
#include "org/minima/system/commands/base/coinexport.hpp"
#include "org/minima/system/commands/base/coinimport.hpp"
#include "org/minima/system/commands/base/coinnotify.hpp"
#include "org/minima/system/commands/base/cointrack.hpp"
#include "org/minima/system/commands/base/consolidate.hpp"
#include "org/minima/system/commands/base/convert.hpp"
#include "org/minima/system/commands/base/debugflag.hpp"
#include "org/minima/system/commands/base/getaddress.hpp"
#include "org/minima/system/commands/base/hash.hpp"
#include "org/minima/system/commands/base/hashtest.hpp"
#include "org/minima/system/commands/base/healthcheck.hpp"
#include "org/minima/system/commands/base/incentivecash.hpp"
#include "org/minima/system/commands/base/logs.hpp"
#include "org/minima/system/commands/base/maths.hpp"
#include "org/minima/system/commands/base/mempool.hpp"
#include "org/minima/system/commands/base/mmrcreate.hpp"
#include "org/minima/system/commands/base/mmrproof.hpp"
#include "org/minima/system/commands/base/newaddress.hpp"
#include "org/minima/system/commands/base/printmmr.hpp"
#include "org/minima/system/commands/base/printtree.hpp"
#include "org/minima/system/commands/base/quit.hpp"
#include "org/minima/system/commands/base/random.hpp"
#include "org/minima/system/commands/base/scanchain.hpp"
#include "org/minima/system/commands/base/seedrandom.hpp"
#include "org/minima/system/commands/base/slavenode.hpp"
#include "org/minima/system/commands/base/status.hpp"
#include "org/minima/system/commands/base/capabilities.hpp"
#include "org/minima/system/commands/base/memreport.hpp"
#include "org/minima/system/commands/base/systemcheck.hpp"
#include "org/minima/system/commands/base/test.hpp"
#include "org/minima/system/commands/base/timemilli.hpp"
#include "org/minima/system/commands/base/tokencreate.hpp"
#include "org/minima/system/commands/base/tokenvalidate.hpp"
#include "org/minima/system/commands/base/trace.hpp"

// Network commands (org::minima::system::commands::network)
#include "org/minima/system/commands/network/connect.hpp"
#include "org/minima/system/commands/network/disconnect.hpp"
#include "org/minima/system/commands/network/message.hpp"
#include "org/minima/system/commands/network/network.hpp"
#include "org/minima/system/commands/network/p2pstate.hpp"
#include "org/minima/system/commands/network/peers.hpp"
#include "org/minima/system/commands/network/ping.hpp"
#include "org/minima/system/commands/network/rpc.hpp"
#include "org/minima/system/commands/network/webhooks.hpp"

// Scripts commands (org::minima::system::commands::scripts)
#include "org/minima/system/commands/scripts/newscript.hpp"
#include "org/minima/system/commands/scripts/removescript.hpp"
#include "org/minima/system/commands/scripts/runscript.hpp"
#include "org/minima/system/commands/scripts/scripts.hpp"

// Search commands (org::minima::system::commands::search)
#include "org/minima/system/commands/search/coins.hpp"
#include "org/minima/system/commands/search/history.hpp"
#include "org/minima/system/commands/search/keys.hpp"
#include "org/minima/system/commands/search/tokens.hpp"
#include "org/minima/system/commands/search/txpow.hpp"

// Send commands (org::minima::system::commands::send)
#include "org/minima/system/commands/send/multisig.hpp"
#include "org/minima/system/commands/send/multisigread.hpp"
#include "org/minima/system/commands/send/send.hpp"
#include "org/minima/system/commands/send/sendnosign.hpp"
#include "org/minima/system/commands/send/sendpoll.hpp"
#include "org/minima/system/commands/send/sendpost.hpp"
#include "org/minima/system/commands/send/sendsign.hpp"
#include "org/minima/system/commands/send/sendview.hpp"

// Send Wallet commands (org::minima::system::commands::send::wallet)
#include "org/minima/system/commands/send/wallet/consolidatefrom.hpp"
#include "org/minima/system/commands/send/wallet/constructfrom.hpp"
#include "org/minima/system/commands/send/wallet/createfrom.hpp"
#include "org/minima/system/commands/send/wallet/postfrom.hpp"
#include "org/minima/system/commands/send/wallet/rawfrom.hpp"
#include "org/minima/system/commands/send/wallet/sendfrom.hpp"
#include "org/minima/system/commands/send/wallet/signfrom.hpp"

// Signatures commands (org::minima::system::commands::signatures)
#include "org/minima/system/commands/signatures/sign.hpp"
#include "org/minima/system/commands/signatures/verify.hpp"

// Txn commands (org::minima::system::commands::txn)
#include "org/minima/system/commands/txn/txnaddamount.hpp"
#include "org/minima/system/commands/txn/txnauto.hpp"
#include "org/minima/system/commands/txn/txnbasics.hpp"
#include "org/minima/system/commands/txn/txncheck.hpp"
#include "org/minima/system/commands/txn/txnclear.hpp"
#include "org/minima/system/commands/txn/txncoinlock.hpp"
#include "org/minima/system/commands/txn/txncreate.hpp"
#include "org/minima/system/commands/txn/txndelete.hpp"
#include "org/minima/system/commands/txn/txnexport.hpp"
#include "org/minima/system/commands/txn/txnimport.hpp"
#include "org/minima/system/commands/txn/txninput.hpp"
#include "org/minima/system/commands/txn/txnlist.hpp"
#include "org/minima/system/commands/txn/txnlock.hpp"
#include "org/minima/system/commands/txn/txnmine.hpp"
#include "org/minima/system/commands/txn/txnminepost.hpp"
#include "org/minima/system/commands/txn/txnmmr.hpp"
#include "org/minima/system/commands/txn/txnoutput.hpp"
#include "org/minima/system/commands/txn/txnpost.hpp"
#include "org/minima/system/commands/txn/txnscript.hpp"
#include "org/minima/system/commands/txn/txnsign.hpp"
#include "org/minima/system/commands/txn/txnstate.hpp"
#include "org/minima/system/commands/txn/txnview.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {

using org::minima::utils::json::JSONObject;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::parser::JSONParser;
using org::minima::utils::json::parser::ParseException;

std::unique_ptr<CommandRunner> CommandRunner::getRunner() {
    return std::unique_ptr<CommandRunner>(new CommandRunner());
}

CommandRunner::CommandRunner() = default;

std::unique_ptr<JSONArray> CommandRunner::runMultiCommand(const std::string& zCommand) {
    return runMultiCommand("0x00", zCommand);
}

std::unique_ptr<JSONObject> CommandRunner::runSingleCommand(const std::string& zCommand) {
    // Java: runMultiCommand(zCommand) then return first JSONObject
    auto arr = runMultiCommand(zCommand);
    if (!arr || arr->size() == 0) {
        auto out = std::make_unique<JSONObject>();
        out->put("status", false);
        out->put("error", std::string("No result"));
        return out;
    }
    const std::any& first = arr->at(0);
    // Expect we stored shared_ptr<JSONObject>
    if (auto pobj = std::any_cast<std::shared_ptr<JSONObject>>(&first)) {
        return std::make_unique<JSONObject>(**pobj);
    }
    // Fallback: if stored as direct JSONObject
    if (auto jobj = std::any_cast<JSONObject>(&first)) {
        return std::make_unique<JSONObject>(*jobj);
    }
    // Unknown type
    auto out = std::make_unique<JSONObject>();
    out->put("status", false);
    out->put("error", std::string("Invalid result type"));
    return out;
}

std::unique_ptr<JSONArray> CommandRunner::runMultiCommand(const std::string& zMiniDAPPID, const std::string& zCommand) {
    auto finalresult = std::make_unique<JSONArray>();

    // Split on ';' (Java's StringTokenizer)
    std::size_t start = 0;
    while (start <= zCommand.size()) {
        std::size_t pos = zCommand.find(';', start);
        std::string command = trim(zCommand.substr(start, (pos == std::string::npos) ? std::string::npos : (pos - start)));

        if (!command.empty()) {
            auto cmd = getCommand(command);
            cmd->setMiniDAPPID(zMiniDAPPID);

            std::unique_ptr<JSONObject> result;
            try {
                result = cmd->runCommand();
            } catch (const CommandException& cexc) {
                result = cmd->getJSONReply();
                result->put("status", false);
                // std::string, not const char*: the JSON writer only handles
                // std::string in std::any, so a raw char* silently serialised
                // to an empty error.
                result->put("error", std::string(cexc.what()));
            } catch (const std::exception& exc) {
                org::minima::utils::MinimaLogger::log(exc);
                result = cmd->getJSONReply();
                result->put("status", false);
                result->put("error", std::string(exc.what()));
            }

            // Add it to final array
            std::shared_ptr<JSONObject> resptr(std::move(result));
            finalresult->add(resptr);

            // Stop at a false status
            try {
                if (!resptr->getBoolean("status")) {
                    break;
                }
            } catch (...) {
                // If status missing or not boolean, continue
            }
        }

        if (pos == std::string::npos) break;
        start = pos + 1;
    }

    return finalresult;
}

std::unique_ptr<Command> CommandRunner::getCommandOnly(const std::string& zCommandName) {
    auto& protos = getPrototypes();
    for (auto& p : protos) {
        if (p->getName() == zCommandName) {
            Command* fresh = p->getFunction();
            return std::unique_ptr<Command>(fresh);
        }
    }
    return nullptr;
}

std::unique_ptr<Command> CommandRunner::getCommand(const std::string& zCommand) {
    auto& protos = getPrototypes();

    // Split into tokens
    std::vector<std::string> split = splitStringJSON(false, zCommand);
    if (split.empty()) {
        // No tokens -> missing
        return std::unique_ptr<Command>(new org::minima::system::commands::base::missingcmd("", "Command not found"));
    }

    std::string command = toLower(split[0]);

    // Find prototype by name
    Command* raw = nullptr;
    for (auto& p : protos) {
        if (p->getName() == command) {
            raw = p->getFunction();
            break;
        }
    }

    std::unique_ptr<Command> comms;
    if (!raw) {
        comms.reset(new org::minima::system::commands::base::missingcmd(command, "Command not found"));
        return comms;
    } else {
        comms.reset(raw);
    }

    // Set complete command
    comms->setCompleteCommand(zCommand);

    // Parse parameters
    for (std::size_t i = 1; i < split.size(); ++i) {
        const std::string& token = split[i];

        // Find ':'
        std::size_t index = token.find(':');
        if (index == std::string::npos) {
            return std::unique_ptr<Command>(new org::minima::system::commands::base::missingcmd(
                command, "Invalid parameters for " + command + " @ " + token));
        }

        std::string name  = trim(token.substr(0, index));
        std::string value = trim(token.substr(index + 1));

        // Is the value a JSONObject.. or JSONArray..
        if (!value.empty() && value.front() == '{' && value.back() == '}') {
            // It's a JSON object
            try {
                JSONParser parser;
                std::any anyv = parser.parse(value);
                // Expect a std::shared_ptr<JSONObject>
                if (auto jobj = std::any_cast<std::shared_ptr<JSONObject>>(&anyv)) {
                    comms->getParams().put(name, **jobj);
                } else if (auto vobj = std::any_cast<JSONObject>(&anyv)) {
                    comms->getParams().put(name, *vobj);
                } else {
                    // Fallback: store raw string if type unexpected
                    comms->getParams().put(name, value);
                }
            } catch (const ParseException& e) {
                return std::unique_ptr<Command>(
                    new org::minima::system::commands::base::missingcmd(
                        command, "Invalid JSON parameter for " + command + " @ " + token + " " + e.getMessage()));
            }
        } else if (!value.empty() && value.front() == '[' && value.back() == ']') {
            // JSONArray?
            if (command == "txnstate") {
                // Store as string in txnstate case
                comms->getParams().put(name, value);
                continue;
            }
            try {
                JSONParser parser;
                std::any anyv = parser.parse(value);
                // Expect a std::shared_ptr<JSONArray>
                if (auto jarr = std::any_cast<std::shared_ptr<JSONArray>>(&anyv)) {
                    comms->getParams().put(name, **jarr);
                } else if (auto varr = std::any_cast<JSONArray>(&anyv)) {
                    comms->getParams().put(name, *varr);
                } else {
                    // Otherwise it's a broken JSONArray
                    return std::unique_ptr<Command>(
                        new org::minima::system::commands::base::missingcmd(
                            command, "Invalid JSONArray parameter for " + command + " @ " + token));
                }
            } catch (const ParseException& e) {
                return std::unique_ptr<Command>(
                    new org::minima::system::commands::base::missingcmd(
                        command, "Invalid JSONArray parameter for " + command + " @ " + token + " " + e.getMessage()));
            }
        } else {
            // Normal String parameter: strip surrounding quotes if present
            if (!value.empty() && value.front() == '"') {
                value = value.substr(1);
            }
            if (!value.empty() && value.back() == '"') {
                value.pop_back();
            }
            comms->getParams().put(name, value);
        }
    }

    return comms;
}

static const std::unordered_set<std::string>& getWriteCommands() {
    static const std::unordered_set<std::string> s_write = {
        "send","sendpoll","sendsign","multisig","tokencreate","consolidate",
        "cointrack","sign","txnsign","backup","removescript",
        "restore","restoresync","vault","archive","mysql","mysqlcoins",
        "rpc","magic","quit","seedrandom","megammrsync"
    };
    return s_write;
}

bool CommandRunner::isCommandAllowed(const std::string& zCommand) {
    const auto& writes = getWriteCommands();
    std::string cmd = trim(zCommand);
    return writes.find(cmd) == writes.end();
}

namespace {
    namespace cmd     = org::minima::system::commands;
    namespace backup  = org::minima::system::commands::backup;
    namespace mmrsync = org::minima::system::commands::backup::mmrsync;
    namespace base    = org::minima::system::commands::base;
    namespace network = org::minima::system::commands::network;
    namespace scripts = org::minima::system::commands::scripts;
    namespace search  = org::minima::system::commands::search;
    namespace send    = org::minima::system::commands::send;
    namespace wallet  = org::minima::system::commands::send::wallet;
    namespace sigs    = org::minima::system::commands::signatures;
    namespace txn     = org::minima::system::commands::txn;
}

std::vector<std::unique_ptr<Command>>& CommandRunner::getPrototypes() {
    static std::vector<std::unique_ptr<Command>> s_protos;
    
    // Populate the list ONCE, just like the Java static final array
    if (s_protos.empty()) {
        s_protos.reserve(114); // Reserve space for all commands

        // The order here MUST match CommandRunner.java ALL_COMMANDS array
        s_protos.push_back(std::make_unique<base::quit>());
        s_protos.push_back(std::make_unique<base::status>());
        s_protos.push_back(std::make_unique<base::capabilities>());
        s_protos.push_back(std::make_unique<base::memreport>());
        s_protos.push_back(std::make_unique<search::coins>());
        s_protos.push_back(std::make_unique<search::txpow>());
        s_protos.push_back(std::make_unique<network::connect>());
        s_protos.push_back(std::make_unique<network::disconnect>());
        s_protos.push_back(std::make_unique<network::network>());
        s_protos.push_back(std::make_unique<network::message>());
        s_protos.push_back(std::make_unique<base::trace>());
        s_protos.push_back(std::make_unique<cmd::help>());
        s_protos.push_back(std::make_unique<base::printtree>());
        s_protos.push_back(std::make_unique<base::automine>());
        s_protos.push_back(std::make_unique<base::printmmr>());
        s_protos.push_back(std::make_unique<network::rpc>());
        s_protos.push_back(std::make_unique<send::send>());
        s_protos.push_back(std::make_unique<base::balance>());
        s_protos.push_back(std::make_unique<base::tokencreate>());
        s_protos.push_back(std::make_unique<base::tokenvalidate>());
        s_protos.push_back(std::make_unique<search::tokens>());
        s_protos.push_back(std::make_unique<base::getaddress>());
        s_protos.push_back(std::make_unique<base::newaddress>());
        s_protos.push_back(std::make_unique<base::debugflag>());
        s_protos.push_back(std::make_unique<base::incentivecash>());
        s_protos.push_back(std::make_unique<network::webhooks>());
        s_protos.push_back(std::make_unique<network::peers>());
        s_protos.push_back(std::make_unique<network::p2pstate>());
        s_protos.push_back(std::make_unique<send::sendpoll>());
        s_protos.push_back(std::make_unique<base::healthcheck>());
        s_protos.push_back(std::make_unique<base::mempool>());
        s_protos.push_back(std::make_unique<base::block>());
        s_protos.push_back(std::make_unique<backup::reset>());
        s_protos.push_back(std::make_unique<cmd::whitepaper>());
        s_protos.push_back(std::make_unique<send::sendnosign>());
        s_protos.push_back(std::make_unique<send::sendsign>());
        s_protos.push_back(std::make_unique<send::sendpost>());
        s_protos.push_back(std::make_unique<send::sendview>());
        s_protos.push_back(std::make_unique<wallet::sendfrom>());
        s_protos.push_back(std::make_unique<wallet::createfrom>());
        s_protos.push_back(std::make_unique<wallet::rawfrom>());
        s_protos.push_back(std::make_unique<wallet::signfrom>());
        s_protos.push_back(std::make_unique<wallet::postfrom>());
        s_protos.push_back(std::make_unique<wallet::constructfrom>());
        s_protos.push_back(std::make_unique<wallet::consolidatefrom>());
        s_protos.push_back(std::make_unique<backup::archive>());
        s_protos.push_back(std::make_unique<base::logs>());
        s_protos.push_back(std::make_unique<search::history>());
        s_protos.push_back(std::make_unique<base::convert>());
        s_protos.push_back(std::make_unique<base::maths>());
        s_protos.push_back(std::make_unique<backup::restoresync>());
        s_protos.push_back(std::make_unique<base::timemilli>());
        s_protos.push_back(std::make_unique<backup::decryptbackup>());
        s_protos.push_back(std::make_unique<mmrsync::megammrsync>());
        s_protos.push_back(std::make_unique<base::systemcheck>());
        s_protos.push_back(std::make_unique<base::scanchain>());
        s_protos.push_back(std::make_unique<send::multisig>());
        s_protos.push_back(std::make_unique<send::multisigread>());
        s_protos.push_back(std::make_unique<base::checkaddress>());
        s_protos.push_back(std::make_unique<network::ping>());
        s_protos.push_back(std::make_unique<base::random>());
        s_protos.push_back(std::make_unique<base::seedrandom>());
        s_protos.push_back(std::make_unique<backup::mysql>());
        s_protos.push_back(std::make_unique<backup::mysqlcoins>());
        s_protos.push_back(std::make_unique<base::slavenode>());
        s_protos.push_back(std::make_unique<mmrsync::megammr>());
        s_protos.push_back(std::make_unique<backup::vault>());
        s_protos.push_back(std::make_unique<base::consolidate>());
        s_protos.push_back(std::make_unique<base::coinnotify>());
        s_protos.push_back(std::make_unique<backup::backup>());
        s_protos.push_back(std::make_unique<backup::restore>());
        s_protos.push_back(std::make_unique<base::test>());
        s_protos.push_back(std::make_unique<scripts::runscript>());
        s_protos.push_back(std::make_unique<cmd::tutorial>());
        s_protos.push_back(std::make_unique<search::keys>());
        s_protos.push_back(std::make_unique<scripts::scripts>());
        s_protos.push_back(std::make_unique<scripts::newscript>());
        s_protos.push_back(std::make_unique<scripts::removescript>());
        s_protos.push_back(std::make_unique<base::burn>());
        s_protos.push_back(std::make_unique<txn::txnbasics>());
        s_protos.push_back(std::make_unique<txn::txncreate>());
        s_protos.push_back(std::make_unique<txn::txninput>());
        s_protos.push_back(std::make_unique<txn::txnlist>());
        s_protos.push_back(std::make_unique<txn::txnclear>());
        s_protos.push_back(std::make_unique<txn::txnview>());
        s_protos.push_back(std::make_unique<txn::txnoutput>());
        s_protos.push_back(std::make_unique<txn::txnstate>());
        s_protos.push_back(std::make_unique<txn::txnsign>());
        s_protos.push_back(std::make_unique<txn::txnpost>());
        s_protos.push_back(std::make_unique<txn::txndelete>());
        s_protos.push_back(std::make_unique<txn::txnexport>());
        s_protos.push_back(std::make_unique<txn::txnimport>());
        s_protos.push_back(std::make_unique<txn::txncheck>());
        s_protos.push_back(std::make_unique<txn::txnscript>());
        s_protos.push_back(std::make_unique<txn::txnauto>());
        s_protos.push_back(std::make_unique<txn::txnaddamount>());
        s_protos.push_back(std::make_unique<txn::txnlock>());
        s_protos.push_back(std::make_unique<txn::txnmmr>());
        s_protos.push_back(std::make_unique<txn::txnmine>());
        s_protos.push_back(std::make_unique<txn::txnminepost>());
        s_protos.push_back(std::make_unique<txn::txncoinlock>());
        s_protos.push_back(std::make_unique<base::coinimport>());
        s_protos.push_back(std::make_unique<base::coinexport>());
        s_protos.push_back(std::make_unique<base::cointrack>());
        s_protos.push_back(std::make_unique<base::coincheck>());
        s_protos.push_back(std::make_unique<base::hash>());
        s_protos.push_back(std::make_unique<base::hashtest>());
        s_protos.push_back(std::make_unique<sigs::sign>());
        s_protos.push_back(std::make_unique<sigs::verify>());
        s_protos.push_back(std::make_unique<base::mmrcreate>());
        s_protos.push_back(std::make_unique<base::mmrproof>());
    }

    return s_protos;
}

std::vector<std::string> CommandRunner::splitStringJSON(bool zForceNormal, const std::string& zInput) {
    // Fast path if possible
    if (!zForceNormal) {
        bool iswindowsfile = (zInput.find(":\\") != std::string::npos);
        if (!iswindowsfile && zInput.find('{') == std::string::npos && zInput.find('[') == std::string::npos) {
            std::vector<std::string> fastres = splitterQuotedPattern(zInput);
            if (!fastres.empty()) {
                return fastres;
            }
        }
    }

    std::vector<std::string> token;
    std::string ss = trim(zInput);

    std::string current;
    int jsoned = 0;
    bool quoted = false;

    for (char cc : ss) {
        if (cc == ' ') {
            if (!quoted && jsoned == 0) {
                if (!current.empty()) {
                    token.push_back(trim(current));
                }
                current.clear();
            } else {
                current += cc;
            }
        } else if (cc == '{') {
            jsoned++;
            current += cc;
        } else if (cc == '}') {
            jsoned--;
            current += cc;
        } else if (cc == '[') {
            jsoned++;
            current += cc;
        } else if (cc == ']') {
            jsoned--;
            current += cc;
        } else if (cc == '\"') {
            if (jsoned > 0) {
                current += cc;
            } else {
                quoted = !quoted;
            }
        } else {
            current += cc;
        }
    }

    if (!current.empty()) {
        token.push_back(trim(current));
    }

    return token;
}

// Very fast splitter for non-JSON inputs (mirrors Java regex method)

// Linear equivalent of the Java regex "\"([^\"]*)\"|(\S+)" used for command
// tokenising. std::regex recursed per character and overflowed the stack on
// long values (e.g. a serialized transaction in data:0x...).
static std::vector<std::string> splitQuotedPatternLinear(const std::string& ss) {
    std::vector<std::string> token;
    const std::size_t n = ss.size();
    std::size_t i = 0;
    while (i < n) {
        while (i < n && std::isspace(static_cast<unsigned char>(ss[i]))) ++i;
        if (i >= n) break;
        if (ss[i] == '"') {
            const std::size_t close = ss.find('"', i + 1);
            if (close != std::string::npos) {
                token.emplace_back(ss.substr(i + 1, close - i - 1));
                i = close + 1;
                continue;
            }
            // Unterminated quote: the regex fallback (\S+) matches from here.
        }
        const std::size_t start = i;
        while (i < n && !std::isspace(static_cast<unsigned char>(ss[i]))) ++i;
        token.emplace_back(ss.substr(start, i - start));
    }
    return token;
}

std::vector<std::string> CommandRunner::splitterQuotedPattern(const std::string& zInput) {
    std::vector<std::string> token;

    // Replace ':' with ' : ' to split name/value
    std::string ss = trim(zInput);
    {
        std::string replaced;
        replaced.reserve(ss.size() * 2);
        for (std::size_t i = 0; i < ss.size(); ++i) {
            char c = ss[i];
            if (c == ':') {
                replaced.append(" : ");
            } else {
                replaced.push_back(c);
            }
        }
        ss.swap(replaced);
    }

    token = splitQuotedPatternLinear(ss);

    // Now build name:value pairs
    std::vector<std::string> finaltokens;
    bool first = true;
    bool namefound = false;
    std::string nvpair;

    for (const auto& tok : token) {
        if (first) {
            first = false;
            finaltokens.push_back(tok);
        } else {
            if (!namefound) {
                if (trim(tok) == ":") {
                    // Unexpected; fail and let slow method handle it
                    return {};
                }
                namefound = true;
                nvpair = tok;
            } else {
                if (trim(tok) == ":") {
                    nvpair += ":";
                } else {
                    namefound = false;
                    nvpair += tok;
                    finaltokens.push_back(nvpair);
                }
            }
        }
    }

    return finaltokens;
}

std::string CommandRunner::trim(const std::string& s) {
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string CommandRunner::toLower(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return out;
}

// ---------------------------------------------------------------------------
// Capability manifest (RFC-0011 §5). Generated from the registry.
// ---------------------------------------------------------------------------
namespace {
std::string deriveCategory(const std::string& name) {
    auto has = [&](const char* p) { return name.rfind(p, 0) == 0; };
    if (has("mash")) return "mash";
    if (has("txn")) return "txn";
    if (has("send")) return "send";
    if (has("lookup")) return "lookup";
    if (has("connect") || has("disconnect") || name == "peers" || name == "network" ||
        name == "nodecount" || name == "ping" || name == "message") return "network";
    if (has("backup") || has("restore") || has("reset") || has("vault") || has("archive") ||
        has("duckdb") || has("megammr") || has("coinimport") || has("coinexport")) return "backup";
    if (has("wallet") || name == "getaddress" || name == "newaddress" ||
        name == "balance" || name == "coins") return "wallet";
    return "base";
}

std::string currentModeName() {
    using org::minima::system::params::GeneralParams;
    if (GeneralParams::TXBLOCK_NODE) return "txblock";
    if (GeneralParams::IS_MEGAMMR) return "megammr";
    if (GeneralParams::ARCHIVE) return "archive";
    return "full";
}
} // namespace

std::unique_ptr<org::minima::utils::json::JSONObject> CommandRunner::capabilityManifest() {
    using org::minima::utils::json::JSONArray;
    using org::minima::utils::json::JSONObject;

    auto root = std::make_unique<JSONObject>();

    // Node header: which node is this, and which SDK domains it serves.
    JSONObject node;
    node.put("version", org::minima::system::params::GlobalParams::getFullMicroVersion());
    node.put("network", org::minima::system::brains::TxPoWChecker::CURRENT_NETWORK.to0xString());
    const std::string mode = currentModeName();
    node.put("mode", mode);

    JSONArray serves;
    serves.add(std::string("statechain"));
    serves.add(std::string("identity"));
    if (mode == "archive") serves.add(std::string("archive"));
    if (mode == "megammr") serves.add(std::string("megammr"));
    if (mode == "txblock") serves.add(std::string("txblock"));
    serves.add(std::string("lookup"));
    node.put("serves", serves);

    JSONObject mash;
    mash.put("enabled", false);
    node.put("mash", mash);

    root->put("node", node);

    // Commands: name, summary, category, annotations, typed params.
    JSONArray cmds;
    for (auto& p : getPrototypes()) {
        if (!p) continue;
        const std::string name = p->getName();
        JSONObject c;
        c.put("name", name);
        c.put("summary", p->getHelp());

        const CommandAnnotations a = p->getAnnotations();
        c.put("category", a.category.empty() ? deriveCategory(name) : a.category);
        JSONObject ann;
        ann.put("readOnly", a.readOnly);
        ann.put("destructive", a.destructive);
        ann.put("idempotent", a.idempotent);
        ann.put("sensitive", a.sensitive);
        c.put("annotations", ann);

        JSONArray params;
        for (const auto& s : p->getParamSpecs()) {
            JSONObject ps;
            ps.put("name", s.name);
            ps.put("type", s.type);
            ps.put("required", s.required);
            if (!s.defaultValue.empty()) ps.put("default", s.defaultValue);
            if (!s.description.empty()) ps.put("description", s.description);
            if (!s.enumValues.empty()) {
                JSONArray ev;
                for (const auto& e : s.enumValues) ev.add(e);
                ps.put("enum", ev);
            }
            params.add(ps);
        }
        c.put("params", params);
        cmds.add(c);
    }
    root->put("commands", cmds);
    return root;
}

namespace {
std::string mdCell(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (char c : s) {
        if (c == '|') o += "\\|";
        else if (c == '\n' || c == '\r') o += ' ';
        else o += c;
    }
    return o;
}
} // namespace

std::string CommandRunner::capabilityManifestMarkdown() {
    using org::minima::utils::json::JSONArray;
    using org::minima::utils::json::JSONObject;

    std::unique_ptr<JSONObject> m = capabilityManifest();
    if (!m) return std::string();

    const JSONObject* node = std::any_cast<JSONObject>(&m->get("node"));
    const JSONArray* cmds = std::any_cast<JSONArray>(&m->get("commands"));

    std::ostringstream out;
    out << "# Node Command & Tool Reference\n\n";
    out << "<!-- GENERATED from the node capability manifest (RFC-0011 §5) - do not edit by hand. -->\n";
    out << "<!-- Regenerate with tests/gen_command_reference; tests/test_docs_manifest asserts it matches. -->\n\n";

    if (node) {
        out << "Node: version `" << node->getString("version", "?")
            << "`, network `" << node->getString("network", "?")
            << "`, mode `" << node->getString("mode", "?") << "`.\n\n";
    }

    const std::size_t total = cmds ? cmds->size() : 0;
    out << "Commands: " << total << ".\n\n";

    std::map<std::string, std::vector<JSONObject>> byCat;
    if (cmds) {
        for (const auto& e : cmds->elements()) {
            const JSONObject* c = std::any_cast<JSONObject>(&e);
            if (!c) continue;
            byCat[c->getString("category", "other")].push_back(*c);
        }
    }

    for (auto& kv : byCat) {
        out << "## " << kv.first << "\n\n";
        out << "| Command | Access | Parameters | Summary |\n";
        out << "|---|---|---|---|\n";
        std::sort(kv.second.begin(), kv.second.end(), [](const JSONObject& a, const JSONObject& b) {
            return a.getString("name") < b.getString("name");
        });
        for (const auto& c : kv.second) {
            std::string access = "read";
            if (const JSONObject* ann = std::any_cast<JSONObject>(&c.get("annotations"))) {
                if (ann->getBoolean("destructive")) access = "destructive";
                else if (!ann->getBoolean("readOnly")) access = "write";
            }
            std::string params;
            if (const JSONArray* ps = std::any_cast<JSONArray>(&c.get("params"))) {
                bool first = true;
                for (const auto& pe : ps->elements()) {
                    const JSONObject* p = std::any_cast<JSONObject>(&pe);
                    if (!p) continue;
                    if (!first) params += ", ";
                    first = false;
                    params += "`" + p->getString("name");
                    if (p->getBoolean("required")) params += "*";
                    params += "`";
                }
            }
            if (params.empty()) params = "-";
            out << "| `" << mdCell(c.getString("name")) << "` | " << access << " | " << params
                << " | " << mdCell(c.getString("summary")) << " |\n";
        }
        out << "\n";
    }

    return out.str();
}

} // namespace commands
} // namespace system
} // namespace minima
} // namespace org