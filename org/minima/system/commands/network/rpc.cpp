#include "org/minima/system/commands/network/rpc.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/user_d_b.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/system/main.hpp"
#include "org/minima/system/network/network_manager.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/ssl/s_s_l_manager.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace network {

using org::minima::database::MinimaDB;
using org::minima::database::userprefs::UserDB;
using org::minima::objects::base::MiniData;
using org::minima::system::Main;
using org::minima::system::commands::CommandException;
using org::minima::system::params::GeneralParams;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;
using org::minima::utils::ssl::SSLManager;

static std::string toLowerCopy(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

rpc::rpc()
    : org::minima::system::commands::Command(
          "rpc",
          std::string("(enable:) (ssl:) (password:) - Enable and disable RPC on port ")
              + std::to_string(GeneralParams::RPC_PORT) + " (default is off)") {}

std::string rpc::getFullHelp() const {
    std::string help;
    help += "\nrpc\n";
    help += "\n";
    help += "Enable and disable RPC on port " + std::to_string(GeneralParams::RPC_PORT) + " (default is off).\n";
    help += "\n";
    help += "Ensure your RPC port is secured behind a firewall before enabling.\n";
    help += "\n";
    help += "The Deafult user is minima. That has write mode access. You can add other rpc users with read access.\n";
    help += "\n";
    help += "enable:\n";
    help += "    true or false, true to enable rpc or false to disable.\n";
    help += "\n";
    help += "ssl:\n";
    help += "    true or false, true to enable Self signed SSL - you can use stunnel yourself.\n";
    help += "\n";
    help += "password:\n";
    help += "    the Basic Auth password used in headers - ONLY secure if used with SSL.\n";
    help += "\n";
    help += "username:\n";
    help += "    the Username of an RPC user.\n";
    help += "\n";
    help += "mode:\n";
    help += "    the read or write mode of an RPC user.\n";
    help += "\n";
    help += "action:\n";
    help += "    adduser - add an RPC user.\n";
    help += "    removeuser - remove an RPC user.\n";
    help += "    listusers - list RPC users.\n";
    help += "\n";
    help += "Examples:\n";
    help += "\n";
    help += "rpc enable:true\n";
    help += "\n";
    help += "rpc enable:true ssl:true password:minimarpcpassword\n";
    help += "\n";
    help += "rpc enable:true password:minima\n";
    help += "\n";
    help += "rpc action:adduser username:rpcuser password:rpcpassword mode:read\n";
    help += "\n";
    help += "rpc action:removeuser username:rpcuser\n";
    help += "\n";
    help += "rpc action:listusers\n";
    help += "\n";
    help += "rpc enable:false\n";
    return help;
}

std::vector<std::string> rpc::getValidParams() const {
    // Preserve the exact list from Java (including the duplicate "password")
    return std::vector<std::string>{"enable", "ssl", "password", "action", "username", "password", "mode"};
}

std::unique_ptr<JSONObject> rpc::runCommand() {
    auto ret = getJSONReply();

    UserDB& userdb = MinimaDB::getDB()->getUserDB();
    bool listusers = false;

    if (existsParam("enable")) {
        bool enable = getBooleanParam("enable");

        // Stop the old
        if (Main::getInstance()) {
            Main::getInstance()->getNetworkManager().stopRPC();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // Store
        GeneralParams::RPC_ENABLED = enable;

        // SSL flag
        GeneralParams::RPC_SSL = getBooleanParam("ssl", false);

        // Password / Auth
        if (existsParam("password")) {
            GeneralParams::RPC_PASSWORD = getParam("password");
            GeneralParams::RPC_AUTHENTICATE = true;
        } else {
            GeneralParams::RPC_AUTHENTICATE = false;
        }

        // Now start or stop
        if (Main::getInstance()) {
            if (enable) {
                Main::getInstance()->getNetworkManager().startRPC();
            } else {
                Main::getInstance()->getNetworkManager().stopRPC();
            }
        }

    } else if (existsParam("action")) {
        std::string action = getParam("action");

        if (action == "adduser") {
            if (!GeneralParams::RPC_AUTHENTICATE) {
                throw CommandException(
                    "You must set a default RPC password (for user minima) via -rpcpassword to add extra users");
            }

            std::string username = getParam("username");
            if (username == "minima") {
                throw CommandException("Cannot add User minima - this is the default RPC user.");
            }

            std::string password = getParam("password");
            std::string mode = toLowerCopy(getParam("mode"));
            if (!(mode == "read" || mode == "write")) {
                throw CommandException("RPC User mode MUST be either read or write");
            }

            JSONObject newuser;
            newuser.put("username", username);
            newuser.put("password", password);
            newuser.put("mode", mode);

            // Read-modify-write since C++ API returns JSONArray by value
            JSONArray users = userdb.getRPCUsers();
            users.add(std::any(newuser));
            userdb.setRPCUsers(users);

        } else if (action == "removeuser") {
            std::string username = getParam("username");

            JSONArray users = userdb.getRPCUsers();
            JSONArray newusers;

            for (std::size_t i = 0; i < users.size(); ++i) {
                const std::any& userobj = users.at(i);

                // Expect JSONObject value
                if (userobj.type() == typeid(JSONObject)) {
                    JSONObject user = std::any_cast<JSONObject>(userobj);
                    if (user.getString("username") != username) {
                        newusers.add(std::any(user));
                    }
                } else {
                    // If unexpected type, preserve it (defensive)
                    newusers.add(userobj);
                }
            }

            userdb.setRPCUsers(newusers);

        } else if (action == "listusers") {
            listusers = true;
        }
    }

    JSONObject rpcdets;
    rpcdets.put("enabled", GeneralParams::RPC_ENABLED);
    rpcdets.put("port", GeneralParams::RPC_PORT);
    rpcdets.put("ssl", GeneralParams::RPC_SSL);

    // Attempt to indicate SSL pub key; API doesn't expose cert/public key bytes.
    // We keep the field but cannot populate it with the key via the provided interface.
    std::string sslpubkey;
    try {
        auto ks = SSLManager::getSSLKeyStore();
        if (ks && ks->isValid()) {
            // No API to extract the certificate/public key; set as empty to preserve schema.
            sslpubkey = "";
        } else {
            sslpubkey = "";
        }
    } catch (...) {
        sslpubkey = "";
    }
    rpcdets.put("sslpubkey", sslpubkey);

    rpcdets.put("authenticate", GeneralParams::RPC_AUTHENTICATE);
    rpcdets.put("username", std::string("minima"));
    rpcdets.put("password", std::string("***"));

    if (!listusers) {
        rpcdets.put("rpcusers", static_cast<std::int64_t>(userdb.getRPCUsers().size()));
    } else {
        rpcdets.put("rpcusers", userdb.getRPCUsers());
    }

    ret->put("response", rpcdets);
    return ret;
}

org::minima::system::commands::Command* rpc::getFunction() {
    return new rpc();
}

} // namespace network
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org