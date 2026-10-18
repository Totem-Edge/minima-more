#include "org/minima/system/commands/base/test.hpp"

#include <fstream>
#include <utility>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

test::test()
    : org::minima::system::commands::Command("test", "test Funxtion") {
}

std::vector<std::string> test::getValidParams() const {
    return std::vector<std::string>{ "show", "action" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> test::runCommand() {
    // Prepare reply
    std::unique_ptr<org::minima::utils::json::JSONObject> ret = getJSONReply();

    // Log and refresh DBs
    org::minima::utils::MinimaLogger::log("About to close and reopen DBs");

    org::minima::database::MinimaDB::getDB()->refreshSQLDB();

    org::minima::utils::MinimaLogger::log("DBs reopened..");

    return ret;
}

std::unique_ptr<std::istream> test::getFileFromResourceAsStream(const std::string& fileName) {
    // Try to open from the current working directory (cross-platform)
    auto ifs = std::make_unique<std::ifstream>(fileName, std::ios::in | std::ios::binary);
    if (!ifs->good()) {
        org::minima::utils::MinimaLogger::log(std::string("file not found! ") + fileName);
        return nullptr;
    }
    return std::unique_ptr<std::istream>(std::move(ifs));
}

org::minima::system::commands::Command* test::getFunction() {
    return new test();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org