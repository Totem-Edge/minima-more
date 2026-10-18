#pragma once

#include <string>
#include <vector>
#include <memory>

#include "org/minima/system/commands/command.hpp"

// Namespaced forward declarations for project dependencies (Rule 10)
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; class MiniNumber; class MiniString; } } } }
namespace org { namespace minima { namespace objects { namespace mmr { class MMRData; class MMRProof; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class mmrproof : public org::minima::system::commands::Command {
public:
    // Nested class as in Java (unused functionally but preserved)
    class mmrleafnode {
    public:
        int mEntry = 0;
        std::string mData;
        std::unique_ptr<org::minima::objects::base::MiniData> mHash;

        mmrleafnode() = default;

        // PIMPL fix for unique_ptr to forward-declared type
        ~mmrleafnode();
        mmrleafnode(mmrleafnode&&) noexcept;
        mmrleafnode& operator=(mmrleafnode&&) noexcept;

        mmrleafnode(const mmrleafnode&) = delete;
        mmrleafnode& operator=(const mmrleafnode&) = delete;
    };

    mmrproof();

    // Help and params (non-virtual in base; provided here to mirror Java API)
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Core execute
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org