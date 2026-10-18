#include "org/minima/minima.hpp"

// Include main.hpp EARLY
#include "org/minima/system/main.hpp"

#include <algorithm>
#include <any>
#include <cctype>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <memory>
#include <cstring>
#include <sqlite3.h>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
  extern char** environ;
#endif

// Include other project headers
#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/params/global_params.hpp"
#include "org/minima/system/params/param_configurer.hpp"
#include "org/minima/utils/mini_format.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/minima_uncaught_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/objects/keys/mash_wots_backend.hpp"


/**
 * @brief The GLOBAL C++ entry point for the application.
 */
int main(int argc, char* argv[]) {
    // 1. Convert C-style args to a C++ vector of strings
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) {
        args.push_back(std::string(argv[i]));
    }

    // 2. Install the production WOTS backend (SHA3-256 x4 SIMD kernel + bounded
    //    worker pool) into the Winternitz seams. Process-wide and idempotent
    //    (std::call_once); byte-identical to the scalar path. This lifts every
    //    Winternitz public-key computation under signing load. Set
    //    MSHS2_WOTS_WORKERS / MSHS2_WOTS_JOBS to tune, or MSHS2_WOTS_DISABLE=1
    //    to fall back to the legacy per-call std::async path.
    if (std::getenv("MSHS2_WOTS_DISABLE") == nullptr) {
        try {
            org::minima::objects::keys::mash::installMashWotsBackend();
        } catch (const std::exception& e) {
            std::cerr << "WOTS backend install skipped: " << e.what() << std::endl;
        }
    }

    // 3. Call the Minima class's main function
    try {
        org::minima::Minima::main(args);
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in main: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown unhandled exception in main." << std::endl;
        return 2;
    }

    return 0;
}


// Bring specific names into scope
using org::minima::system::params::GeneralParams;
using org::minima::system::params::GlobalParams;
using org::minima::system::params::ParamConfigurer;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;
// using org::minima::utils::json::JSONValue; // Keep removed
using org::minima::system::commands::CommandRunner;

// ### FIX 2a: Define alias for Main ###
using Main = org::minima::system::Main;


// Helper functions
std::string trim_copy(const std::string& s) {
    auto begin = s.begin();
    auto end = s.end();
    while (begin != end && std::isspace(static_cast<unsigned char>(*begin))) ++begin;
    if (begin == end) return std::string();
    do { --end; } while (std::isspace(static_cast<unsigned char>(*end)) && end != begin);
    return std::string(begin, end + 1);
}

std::string user_home_dir() {
#ifdef _WIN32
    const char* up = std::getenv("USERPROFILE");
    if (up && *up) return std::string(up);
    const char* home = std::getenv("HOME");
    if (home && *home) return std::string(home);
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len > 0) {
        std::filesystem::path p(buf);
        return p.parent_path().string();
    }
    return ".";
#else
    const char* home = std::getenv("HOME");
    if (home && *home) return std::string(home);
    return ".";
#endif
}

void ensure_directories(const std::string& path) {
    try {
        std::filesystem::create_directories(std::filesystem::path(path));
    } catch (...) { /* Ignore */ }
}

std::unordered_map<std::string, std::string> collect_environment() {
    std::unordered_map<std::string, std::string> envmap;
#ifdef _WIN32
    LPCH env = GetEnvironmentStringsA();
    if (!env) return envmap;
    const size_t MAX_ENV_VAR_LENGTH = 65536;
    LPCH cur = env;
    while (*cur != '\0') {
        size_t len = std::strlen(cur);
        if (len > 0 && len < MAX_ENV_VAR_LENGTH) {
            // Safe allocation using the known length
            std::string entry(cur, len); 

            auto pos = entry.find('=');
            if (pos != std::string::npos) {
                std::string key = entry.substr(0, pos);
                std::string val = entry.substr(pos + 1);
                envmap.emplace(std::move(key), std::move(val));
            }
        }
        cur += len + 1;
    }
    FreeEnvironmentStringsA(env);
#else
    if (environ) {
        for (char** e = environ; *e != nullptr; ++e) {
            std::string entry(*e);
            auto pos = entry.find('=');
            if (pos != std::string::npos) {
                std::string key = entry.substr(0, pos);
                std::string val = entry.substr(pos + 1);
                envmap.emplace(std::move(key), std::move(val));
            }
        }
    }
#endif
    return envmap;
}

void print_banner() {
    org::minima::utils::MinimaLogger::log("**********************************************");
    org::minima::utils::MinimaLogger::log("*  __  __  ____  _  _  ____  __  __    __    *");
    org::minima::utils::MinimaLogger::log("* (  \\/  )(_  _)( \\( )(_  _)(  \\/  )  /__\\   *");
    org::minima::utils::MinimaLogger::log("*  )    (  _)(_  )  (  _)(_  )    (  /(__)\\  *");
    org::minima::utils::MinimaLogger::log("* (_/\\/\\_)(____)(_)\\_)(____)(_/\\/\\_)(__)(__) *");
    org::minima::utils::MinimaLogger::log("*                                            *");
    org::minima::utils::MinimaLogger::log("**********************************************");
    org::minima::utils::MinimaLogger::log(
        std::string("Welcome to Pure Minima ") + GlobalParams::getFullMicroVersion() +
        " - for assistance type help. Then press enter.");
}


// atexit callback - requires Main definition
void atexit_shutdown_hook() {
    // ### FIX 2b: Use fully qualified name ###
    org::minima::system::Main* m = org::minima::system::Main::getInstance();
    if (m && !m->isShuttingDown()) {
        org::minima::utils::MinimaLogger::log("[!] Shutdown Hook..");
        m->shutdown();
    }
}


namespace org {
namespace minima {

bool Minima::sIsRunning = true;

Minima::Minima() = default;

void Minima::mainStarter(const std::vector<std::string>& args) {
    std::thread([args]() {
        Minima::main(args);
    }).detach();
}

// ### FIX 2c: Use fully qualified name (matches header) ###
org::minima::system::Main* Minima::getMain() {
    return org::minima::system::Main::getInstance();
}

std::string Minima::runMinimaCMD(const std::string& input) {
    return runMinimaCMD(input, true);
}

std::string Minima::runMinimaCMD(const std::string& zInput, bool zPrettyJSON) {
    std::string input = trim_copy(zInput);
    std::shared_ptr<JSONArray> res_real = CommandRunner::getRunner()->runMultiCommand(input);

    if (!res_real) { return "[]"; }

    std::string result;
    if (zPrettyJSON) {
        result = org::minima::utils::MiniFormat::JSONPretty(*res_real);
    } else {
        if (res_real->size() == 1) {
            const std::any& v = res_real->at(0);
            result = org::minima::utils::json::JSONValue::toJSONString(v);
        } else {
            result = res_real->toJSONString();
        }
    }
    return result;
}

void Minima::main(const std::vector<std::string>& zArgs) {
    sIsRunning = true;
    
    const std::string dataFolder = (std::filesystem::path(user_home_dir()) / ".minima").string();
    const std::string minimaFolder = (std::filesystem::path(dataFolder) / GlobalParams::MINIMA_BASE_VERSION).string();
    
    
    GeneralParams::resetDefaults();
    
    
    GeneralParams::DATA_FOLDER = minimaFolder;

    try {
            ParamConfigurer configurer;
        
            const std::unordered_map<std::string, std::string> env = collect_environment();
        
            configurer.usingConfFile(zArgs);
        
            configurer.usingEnvVariables(env);
        
            configurer.usingProgramArgs(zArgs);
        
            configurer.configure();
        
            if (configurer.shouldExit()) {
                    return;
        }

            ensure_directories(GeneralParams::DATA_FOLDER);
        
        const bool daemon = configurer.isDaemon();
        const bool shutdownhook = configurer.isShutDownHook();
        
        GeneralParams::RPC_PORT = GeneralParams::MINIMA_PORT + 4;
        
            
        print_banner();
        
            
        try {
            if (sqlite3_initialize() != SQLITE_OK) {
                throw std::runtime_error("Failed to initialize SQLite3");
            }
            org::minima::utils::MinimaLogger::log("SQLite3 driver loaded successfully.");
            // We will shut it down on exit
            std::atexit([]() { 
                sqlite3_shutdown(); 
            });

        } catch (const std::exception& e) {
            org::minima::utils::MinimaLogger::log(std::string("[!] CRITICAL: SQLite3 exception: ") + e.what());
            std::exit(1);
        }

        try { /* MySQL */ } catch (...) {}
        
        org::minima::utils::MinimaUncaughtException::Install();

            
        auto main_instance = std::make_unique<org::minima::system::Main>();

        if (shutdownhook) { 
            std::atexit(atexit_shutdown_hook); 
        }

        // ... (rest of your original main function from here) ...
        
        if (daemon) {
            org::minima::utils::MinimaLogger::log("Daemon mode started..");
            while (true) {
                org::minima::system::Main* mo = org::minima::system::Main::getInstance();
                if (mo && mo->isShutdownComplete()) { break; }
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
            return;
        }

        while (sIsRunning) {
            org::minima::system::Main* mo = org::minima::system::Main::getInstance();
            if (!mo || !mo->isRunning()) { break; }

            std::string input;
            if (!std::getline(std::cin, input)) { break; }

            input = trim_copy(input);
            if (!input.empty()) {
                if (org::minima::system::Main::getInstance() == nullptr) { break; }

                std::shared_ptr<JSONArray> res_real = CommandRunner::getRunner()->runMultiCommand(input);
                if (!res_real) {
                    if (input != "quit") { std::cout << "[]" << std::endl; }
                    continue;
                }
                
                // Safety limit for pretty print
                const size_t MAX_PRETTY_PRINT_SIZE = 50000;
                if(res_real->size() > MAX_PRETTY_PRINT_SIZE) {
                    std::cout << "[ { \"status\":false, \"message\":\"Response too large to pretty print\" } ]" << std::endl;
                } else {
                    if (input != "quit") { 
                        std::cout << org::minima::utils::MiniFormat::JSONPretty(*res_real) << std::endl; 
                    }
                }

                bool quit = false;
                const auto& elems = res_real->elements();
                 for (const auto& anyv : elems) {
                    if (anyv.type() == typeid(JSONObject)) {
                        try {
                            const auto& json = std::any_cast<const JSONObject&>(anyv);
                            if (json.containsKey("command")) {
                                const std::any& cmdv = json.get("command");
                                if (cmdv.type() == typeid(std::string)) {
                                    const std::string& cmd = std::any_cast<const std::string&>(cmdv);
                                    if (cmd == "quit") { quit = true; break; }
                                }
                            }
                        } catch (const std::exception& exc) {
                            // Handle cases where element is not a JSONObject or key is missing
                            org::minima::utils::MinimaLogger::log(std::string("Error checking quit command: ") + exc.what());
                        }
                    }
                }
                if (quit) { break; }
            }
        }
        org::minima::utils::MinimaLogger::log("Minima CLI input stopped.. ", false);

    } catch (const ParamConfigurer::UnknownArgumentException& ex) {
        std::cerr << "[CRASH] UnknownArgumentException: " << ex.what() << std::endl;
        std::cout << ex.what() << std::endl;
        std::exit(1);
    } catch (const std::exception& ex) {
        std::cerr << "[CRASH] std::exception: " << ex.what() << std::endl;
        org::minima::utils::MinimaLogger::log(ex.what());
    } catch (...) {
        std::cerr << "[CRASH] Unknown exception in Minima::main" << std::endl;
        try { org::minima::utils::MinimaLogger::log("Unknown exception in Minima::main", true); } catch (...) {}
    }
}

} // namespace minima
} // namespace org