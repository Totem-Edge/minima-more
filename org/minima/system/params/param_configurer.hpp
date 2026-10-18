#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <functional>
#include <stdexcept>

namespace org {
namespace minima {
namespace system {
namespace params {

class ParamConfigurer {
public:
    // Enum of all parameter keys
    enum class ParamKeys {
        data,
        dbpassword,
        basefolder,
        host,
        port,
        rpc,
        rpcenable,
        rpcpassword,
        rpcssl,
        rpccrlf,
        shownetcalls,
        shownetcallsnopoll,
        allowallip,
        archive,
        conf,
        daemon,
        isclient,
        desktop,
        server,
        mobile,
        jnlp,
        showparams,
        nop2p,
        noshutdownhook,
        noconnect,
        p2prootnode,
        p2pnodes,
        p2ploglevelinfo,
        p2plogleveldebug,
        p2p2,
        connect,
        clean,
        nodefaultminidapps,
        nosyncibd,
        syncibdlogs,
        megammr,
        notifyalltxpow,
        slavenode,
        limitbandwidth,
        genesis,
        test,
        solo,
        testchainlength,
        mysqldb,
        mysqldbcoins,
        mysqldbdelay,
        mysqlalltxpow,
        txpowdbstore,
        rescuenode,
        help,
        seed,
        anyseed,
        megaprune,
        megaprunestate,
        megaprunetokens
    };

    // Public API
    ParamConfigurer();

    // Parse "-conf <file>" inside args and load that config file if present
    ParamConfigurer& usingConfFile(const std::vector<std::string>& programArgs);

    // Import environment variables (only those starting with minima_)
    ParamConfigurer& usingEnvVariables(const std::unordered_map<std::string, std::string>& envVariableMap);

    // Parse program args into key/value pairs
    ParamConfigurer& usingProgramArgs(const std::vector<std::string>& programArgs);

    // Apply all collected parameters via their consumers and optionally log them
    ParamConfigurer& configure();

    bool isDaemon() const;
    bool isShutDownHook() const;

    // Static checks
    static bool checkParams(const std::string& zFullParams);
    static bool checkParams(const std::vector<std::string>& zParams);

    // Exception type
    class UnknownArgumentException : public std::runtime_error {
    public:
        explicit UnknownArgumentException(const std::string& arg);
    };

    // Utility Pair as in Java
    template<typename L, typename R>
    struct Pair {
        L left;
        R right;
        Pair(L l, R r) : left(std::move(l)), right(std::move(r)) {}
    };

    // Expose setters so consumers can mutate flags (equivalent to Java lambda mutating instance)
    void setDaemon(bool v);
    void setShutdownHook(bool v);

    void setShouldExit(bool v); 
    bool shouldExit() const; 

private:
    // Storage for parsed params
    std::unordered_map<ParamKeys, std::string> m_paramKeysToArg;
    bool m_daemon;
    bool m_shutdownHook;
    bool m_shouldExit;

    // Helpers
    static std::optional<std::string> lookAheadToNonParamKeyArg(const std::vector<std::string>& programArgs, std::size_t currentIndex);
    static std::optional<ParamKeys> progArgsToParamKey(const std::string& str);

    // ParamKeys helpers (string mapping + metadata)
    static std::optional<ParamKeys> toParamKey(const std::string& keystr);
    static std::string keyString(ParamKeys key);
    static std::string helpString(ParamKeys key);
    static const std::vector<ParamKeys>& allKeys();
};

} // namespace params
} // namespace system
} // namespace minima
} // namespace org