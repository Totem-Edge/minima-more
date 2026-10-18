#include "org/minima/system/commands/network/ping.hpp"

#include <stdexcept>
#include <string>
#include <cstddef>
#include <cctype>

#include "org/minima/system/main.hpp"
#include "org/minima/objects/greeting.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/system/network/minima/n_i_o_manager.hpp"

namespace {

// Simple trim helper equivalent to Java's String.trim()
inline std::string trim_copy(const std::string& s) {
    std::string::size_type start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    std::string::size_type end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

// Helpers to uniformly handle pointer, smart pointer, or by-value returns
template<typename T>
inline bool is_null(const std::shared_ptr<T>& p) { return !p; }
template<typename T>
inline bool is_null(const std::unique_ptr<T>& p) { return !p; }
template<typename T>
inline bool is_null(T* p) { return p == nullptr; }
template<typename T>
inline bool is_null(const T&) { return false; }

template<typename T>
inline T* get_ptr(const std::shared_ptr<T>& p) { return p.get(); }
template<typename T>
inline T* get_ptr(const std::unique_ptr<T>& p) { return p.get(); }
template<typename T>
inline T* get_ptr(T* p) { return p; }
template<typename T>
inline const T* get_ptr(const T& v) { return &v; }

} // anonymous namespace

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace network {

ping::ping()
    : org::minima::system::commands::Command(
          "ping",
          "[host:] - Ping a host and get back Minima Node info") {}

std::string ping::getFullHelp() const {
    return std::string("\nping\n")
           + "\n"
           + "Ping a host and get back Minima Node info.\n"
           + "\n"
           + "Examples:\n"
           + "\n"
           + "ping host:\n";
}

std::vector<std::string> ping::getValidParams() const{
    return std::vector<std::string>{"host"};
}

std::unique_ptr<org::minima::utils::json::JSONObject> ping::runCommand() {
    using org::minima::system::Main;
    using org::minima::utils::json::JSONObject;
    using org::minima::system::commands::CommandException;

    auto ret = getJSONReply();

    std::string host = getParam("host");

    std::size_t index = host.find(':');
    if (index == std::string::npos) {
        // Java returns null when no colon is present
        return nullptr;
    }

    std::string ip = trim_copy(host.substr(0, index));
    std::string ports = trim_copy(host.substr(index + 1));

    int port = 0;
    try {
        port = std::stoi(ports);
    } catch (const std::exception&) {
        throw CommandException(std::string("Invalid port : ") + ports);
    }

    // Call the ping function
    auto* nio = &Main::getInstance()->getNIOManager();

    // Accept pointer/smart-pointer or by-value return types
    auto greet = nio->sendPingMessage(ip, port, false);

    JSONObject resp;
    resp.put("host", ip);
    resp.put("port", port);

    if (is_null(greet)) {
        resp.put("valid", false);
    } else {
        resp.put("valid", true);
        org::minima::objects::Greeting* gptr = get_ptr(greet);
        auto ver = gptr->getVersion().toString();
        resp.put("version", ver);
        resp.put("extradata", gptr->getExtraData());
    }

    ret->put("response", resp);
    return ret;
}

org::minima::system::commands::Command* ping::getFunction() {
    return new ping();
}

} // namespace network
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org