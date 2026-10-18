#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"
#include "org/minima/objects/base/mini_number.hpp"

// Forward declaration for project dependency used in signatures (Rule 10)
namespace org { namespace minima { namespace objects {
class TxPoW;
} } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class burn final : public org::minima::system::commands::Command {
public:
    burn();

    // Help text (matches Java intent)
    std::string getFullHelp() const override;
    // Core execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory, mirrors Java getFunction()
    org::minima::system::commands::Command* getFunction() override;

private:
    // Helper to adjust burn statistics for a single TxPoW
    void checkBurn(const org::minima::objects::TxPoW& zTxPoW, int counter);

    // Median from a vector (sorts the vector in-place, descending, returns middle element or ZERO if empty)
    org::minima::objects::base::MiniNumber getMediaValue(std::vector<org::minima::objects::base::MiniNumber>& zValues);

private:
    // 1 block window
    org::minima::objects::base::MiniNumber mMinburn;
    org::minima::objects::base::MiniNumber mMaxburn;
    org::minima::objects::base::MiniNumber mBurnTot;
    org::minima::objects::base::MiniNumber mBurnCount;
    std::vector<org::minima::objects::base::MiniNumber> mValues;

    // 10 block window
    org::minima::objects::base::MiniNumber mMinburn10;
    org::minima::objects::base::MiniNumber mMaxburn10;
    org::minima::objects::base::MiniNumber mBurnTot10;
    org::minima::objects::base::MiniNumber mBurnCount10;
    std::vector<org::minima::objects::base::MiniNumber> mValues10;

    // 50 block window
    org::minima::objects::base::MiniNumber mMinburn50;
    org::minima::objects::base::MiniNumber mMaxburn50;
    org::minima::objects::base::MiniNumber mBurnTot50;
    org::minima::objects::base::MiniNumber mBurnCount50;
    std::vector<org::minima::objects::base::MiniNumber> mValues50;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org