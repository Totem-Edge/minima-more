#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

// Namespaced forward declarations to avoid heavy includes in header (PITFALL 4)
namespace org { namespace minima { namespace objects {
    class Coin;
    class Token;
    class TxPoW;
} } }

namespace org { namespace minima { namespace objects { namespace base {
    class MiniData;
    class MiniNumber;
} } } }

namespace org { namespace minima { namespace utils { namespace json {
    class JSONObject;
    class JSONArray;
} } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {

class send final : public org::minima::system::commands::Command {
public:
    // Helper container (Java inner class AddressAmount)
    struct AddressAmount {
        org::minima::objects::base::MiniData   mAddress;
        org::minima::objects::base::MiniNumber mAmount;

        AddressAmount(const org::minima::objects::base::MiniData& zAddress,
                      const org::minima::objects::base::MiniNumber& zAmount);
        const org::minima::objects::base::MiniData& getAddress() const;
        const org::minima::objects::base::MiniNumber& getAmount() const;
    };

    send();
    ~send() override = default;

    // Help and params (base methods are not virtual; matching Java API pattern)
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Execute
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;

    // Coin selection utilities (static)
    static std::vector<std::shared_ptr<org::minima::objects::Coin>>
    selectCoins(const std::vector<std::shared_ptr<org::minima::objects::Coin>>& zAllCoins,
                const org::minima::objects::base::MiniNumber& zAmountRequired);

    static std::vector<std::shared_ptr<org::minima::objects::Coin>>
    selectCoins(const std::vector<std::shared_ptr<org::minima::objects::Coin>>& zAllCoins,
                const org::minima::objects::base::MiniNumber& zAmountRequired,
                bool zDebug);

    static std::vector<std::shared_ptr<org::minima::objects::Coin>>
    orderCoins(const std::vector<std::shared_ptr<org::minima::objects::Coin>>& zCoins);
};

} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org