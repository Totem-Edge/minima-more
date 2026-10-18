#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <boost/multiprecision/cpp_int.hpp>

#include "org/minima/system/commands/command.hpp"

// Forward declarations for project dependencies (Pitfall 10)
namespace org { namespace minima { namespace database { class MinimaDB; } } }
namespace org { namespace minima { namespace objects { namespace mmr { class MegaMMR; class MMR; class MMRData; class MMRProof; class MMREntryNumber; } } } }
namespace org { namespace minima { namespace objects { class Coin; class CoinProof; class TxBlock; class TxPoW; } } }
namespace org { namespace minima { namespace objects { class IBD; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; class MiniNumber; } } } }
namespace org { namespace minima { namespace system { class Main; } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org { namespace minima { namespace system { namespace commands { class CommandRunner; } } } }
namespace org { namespace minima { namespace system { namespace params { class GeneralParams; } } } }
namespace org { namespace minima { namespace utils { class MinimaLogger; class MiniFile; class MiniFormat; class MiniUtil; } } }

// Forward declare helper classes in this package (part of project)
namespace org { namespace minima { namespace system { namespace commands { namespace backup { namespace mmrsync {
class MegaMMRBackup;
} } } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {
namespace mmrsync {

class megammr : public org::minima::system::commands::Command {
public:
    megammr();

    // Help/params
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Command execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory (like Java getFunction)
    org::minima::system::commands::Command* getFunction() override;

    // Static integrity checks
    static boost::multiprecision::cpp_int checkMegaMMR(const std::filesystem::path& zMegaMMR);
    static boost::multiprecision::cpp_int checkMegaMMR(org::minima::system::commands::backup::mmrsync::MegaMMRBackup& mmrback);
};

} // namespace mmrsync
} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org