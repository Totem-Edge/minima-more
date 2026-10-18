#include "org/minima/system/commands/backup/reset.hpp"

#include <stdexcept>
#include <utility>

#include "org/minima/system/main.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {

using org::minima::utils::json::JSONObject;
using org::minima::utils::json::JSONArray;

reset::reset()
    : org::minima::system::commands::Command(
          "reset",
          "[archivefile:] [action:] (file:) (password:) (keys:) (keyuses:) - Reset the entire system using an Archive Backup file...") {
}

std::string reset::getFullHelp() const {
    return std::string("\nreset\n"
                       "\n"
                       "Reset your node in various ways. You MUST wait until all your original keys are created before this is allowed.\n"
                       "\n"
                       "archivefile:\n"
                       "    Specify the the archive gzip file. Should be recently exported from an archive node.\n"
                       "\n"
                       "action:\n"
                       "    chainsync: Re-sync all blocks from the archivefile to get back onto the right chain.\n"
                       "               Seed phrase is not required, the private keys will remain unchanged.\n"
                       "    seedsync: Wipe the wallet and re-generate your keys from your seed phrase. Your coins will be restored.\n"
                       "    restore: Restore a backup and re-sync the entire chain from the archivefile.\n"
                       "\n"
                       "file:\n"
                       "    Specify the filename or local path of the backup to restore\n"
                       "\n"
                       "password: (optional)\n"
                       "    Enter the password of the backup \n"
                       "\n"
                       "phrase: (optional)\n"
                       "    Your 24 word seed phrase in double quotes. Use with 'action:seedsync'.\n"
                       "\n"
                       "keys: (optional)\n"
                       "    Number of keys to create if you need to do a seed re-sync. Default is 64. Use with 'action:seedsync'.\n"
                       "\n"
                       "keyuses: (optional)\n"
                       "    How many times at most you used your keys. Use with 'action:seedsync'.\n"
                       "    Every time you re-sync with seed phrase this needs to be higher as Minima Signatures are stateful.\n"
                       "    Defaults to 1000 - the max is 262144 for normal keys.\n"
                       "\n"
                       "Examples:\n"
                       "\n"
                       "reset archivefile:archiveexport-jul23.gz action:chainsync\n"
                       "\n"
                       "reset archivefile:archiveexport-jul23.gz action:seedsync keyuses:1000 phrase:\"ENTER 24 WORDS HERE\"\n"
                       "\n"
                       "reset archivefile:archiveexport-jul23.gz action:restore file:backup-jul23.bak password:Longsecurepassword456\n");
}

std::vector<std::string> reset::getValidParams() const {
    return std::vector<std::string>{
        "action", "archivefile", "file", "password", "phrase", "keys", "keyuses"
    };
}

static std::shared_ptr<JSONObject> anyToJSONObjectPtr(const std::any& val) {
    // Try shared_ptr<JSONObject>
    if (val.type() == typeid(std::shared_ptr<JSONObject>)) {
        return std::any_cast<std::shared_ptr<JSONObject>>(val);
    }
    // Try raw JSONObject value (copy then wrap)
    if (val.type() == typeid(JSONObject)) {
        return std::make_shared<JSONObject>(std::any_cast<JSONObject>(val));
    }
    // Try unique_ptr<JSONObject>
    if (val.type() == typeid(std::unique_ptr<JSONObject>)) {
        const auto& up = std::any_cast<const std::unique_ptr<JSONObject>&>(val);
        if (up) {
            return std::make_shared<JSONObject>(*up);
        }
    }
    // Unknown type
    throw org::minima::system::commands::CommandException("Invalid JSON result type in JSONArray");
}

std::unique_ptr<JSONObject> reset::runCommand() {
    auto ret = getJSONReply();

    // Can only do this if all keys created..
    // Java: vault.checkAllKeysCreated();
    // C++ equivalent using available API:
    if (!org::minima::system::Main::getInstance()->getAllKeysCreated()) {
        throw org::minima::system::commands::CommandException(
            "All original keys must be created before this command is allowed");
    }

    // Get the archive file
    const std::string archivefile = getParam("archivefile");

    // Which action
    const std::string action = getParam("action");

    if (action == "chainsync") {
        // Refactor the command
        std::string command = "archive action:import file:" + archivefile;

        auto res = org::minima::system::commands::CommandRunner::getRunner()->runMultiCommand(command);
        if (!res || res->size() == 0) {
            throw org::minima::system::commands::CommandException("No response from archive import");
        }
        auto result = anyToJSONObjectPtr(res->at(0));
        ret->put("response", result);

    } else if (action == "seedsync") {
        // Get the phrase
        const std::string phrase = getParam("phrase");

        // Refactor the command
        std::string command = "archive action:import file:" + archivefile + " phrase:\"" + phrase + "\"";

        // Add extra params
        if (existsParam("keys")) {
            command += " keys:" + getParam("keys");
        }
        if (existsParam("keyuses")) {
            command += " keyuses:" + getParam("keyuses");
        }

        auto res = org::minima::system::commands::CommandRunner::getRunner()->runMultiCommand(command);
        if (!res || res->size() == 0) {
            throw org::minima::system::commands::CommandException("No response from archive seed import");
        }
        auto result = anyToJSONObjectPtr(res->at(0));
        ret->put("response", result);

    } else if (action == "restore") {
        // Get the backup file
        const std::string backupfile = getParam("file");

        // Now do a restore..
        std::string command = "restore shutdown:false file:" + backupfile;

        // Password ?
        if (existsParam("password")) {
            command += " password:" + getParam("password");
        }

        auto res = org::minima::system::commands::CommandRunner::getRunner()->runMultiCommand(command);
        if (!res || res->size() == 0) {
            throw org::minima::system::commands::CommandException("No response from restore");
        }
        auto result = anyToJSONObjectPtr(res->at(0));

        // Check worked..
        if (!result->getBoolean("status")) {
            throw org::minima::system::commands::CommandException(
                std::string("Error restoring.. ") + result->toJSONString());
        }

        auto allres = std::make_shared<JSONObject>();
        allres->put("restore", result);

        // Now reopen the required SQL Dbs..
        org::minima::system::Main::getInstance()->restoreReadyForSync();

        // And NOW do a chain resync..
        command = "archive action:import file:" + archivefile;
        res = org::minima::system::commands::CommandRunner::getRunner()->runMultiCommand(command);
        if (!res || res->size() == 0) {
            throw org::minima::system::commands::CommandException("No response from archive import after restore");
        }
        auto result2 = anyToJSONObjectPtr(res->at(0));
        allres->put("chainsync", result2);

        // The final results..
        ret->put("response", allres);

    } else {
        throw org::minima::system::commands::CommandException(std::string("Invalid action : ") + action);
    }

    return ret;
}

org::minima::system::commands::Command* reset::getFunction() {
    return new reset();
}

} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org