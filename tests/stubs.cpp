/**
 * stubs.cpp - Minimal stub implementations for symbols needed by test_serialization
 * and test_mmr but not actually called during the tests.
 */
#include <string>
#include <exception>
#include <memory>
#include <cstdlib>

namespace org { namespace minima { namespace utils {
class MiniFormat {
public:
    static std::string formatSize(long long v);
};

class MinimaLogger {
public:
    static void log(const std::string& msg);
    static void log(const std::exception& e);
};

// Out-of-line definitions
std::string MiniFormat::formatSize(long long v) {
    return std::to_string(v) + " bytes";
}
void MinimaLogger::log(const std::string&) {}
void MinimaLogger::log(const std::exception&) {}

}}}  // namespace org::minima::utils

namespace org { namespace minima { namespace objects { namespace mmr {
class MegaMMR {
public:
    class MMR;
    MMR& getMMR();
};
MegaMMR::MMR& MegaMMR::getMMR() {
    // This stub is never called by tests because IS_MEGAMMR stays false.
    std::abort();
}
}}}}  // namespace org::minima::objects::mmr

namespace org { namespace minima { namespace database {
class MinimaDB {
public:
    static MinimaDB* getDB();
    org::minima::objects::mmr::MegaMMR& getMegaMMR();
};
MinimaDB* MinimaDB::getDB() { return nullptr; }
org::minima::objects::mmr::MegaMMR& MinimaDB::getMegaMMR() {
    static org::minima::objects::mmr::MegaMMR s_dummy;
    return s_dummy;
}
}}}  // namespace org::minima::database

// Force instantiation
void __force_stubs() {
    org::minima::utils::MiniFormat::formatSize(0);
    org::minima::utils::MinimaLogger::log(std::string());
    org::minima::utils::MinimaLogger::log(std::exception());
    org::minima::database::MinimaDB::getDB();
    org::minima::database::MinimaDB{}.getMegaMMR();
}
