#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

// Forward declarations (Pitfall 10)
namespace org { namespace minima { namespace database { namespace archive { class ArchiveManager; class RawArchiveInput; } } } }
namespace org { namespace minima { namespace utils { namespace messages { class MessageListener; } } } }
namespace org { namespace minima { namespace objects { class IBD; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniNumber; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {

class archive : public org::minima::system::commands::Command {
public:
    archive();
    virtual ~archive() = default;

    // Help and params (base methods are not virtual; do not mark override)
    std::vector<std::string> getValidParams() const;
    std::string getFullHelp() const override;
    // Execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory (clone)
    org::minima::system::commands::Command* getFunction() override;

    // Static helper to notify a MessageListener
    static void NotifyListener(org::minima::utils::messages::MessageListener* zListener,
                               const std::string& zMessage);

    // Static network helper (overloads)
    static std::unique_ptr<org::minima::objects::IBD>
    sendArchiveReq(const std::string& zHost, int zPort,
                   const org::minima::objects::base::MiniNumber& zStartBlock);

    static std::unique_ptr<org::minima::objects::IBD>
    sendArchiveReq(const std::string& zHost, int zPort,
                   const org::minima::objects::base::MiniNumber& zStartBlock,
                   int zAttempts);

private:
    // Constants
    const std::string LOCAL_ARCHIVE = "archiverestore";

    // Static state for local imports
    static bool H2_TEMPARCHIVE;
    static std::unique_ptr<org::minima::database::archive::ArchiveManager> STATIC_TEMPARCHIVE;
    static std::unique_ptr<org::minima::database::archive::RawArchiveInput> STATIC_RAW;

    // Helpers
    static std::string currentTimeString(long long ms);
};

} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org