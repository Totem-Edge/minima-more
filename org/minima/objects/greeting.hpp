#pragma once

#include <memory>
#include <vector>
#include <string>
#include <cstdint>

#include "org/minima/utils/streamable.hpp"

// Forward declarations for project classes used in members/signatures
namespace org { namespace minima { namespace objects { namespace base { class MiniString; class MiniNumber; class MiniData; } } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace objects {

class Greeting : public org::minima::utils::Streamable {
public:
    Greeting();

    // PIMPL-FIX for unique_ptr to forward-declared types
    virtual ~Greeting();
    Greeting(Greeting&&) noexcept;
    Greeting& operator=(Greeting&&) noexcept;
    Greeting(const Greeting&) = delete;
    Greeting& operator=(const Greeting&) = delete;

    // Create the complete greeting message (locks DB, populates fields)
    Greeting& createGreeting();

    // Accessors
    org::minima::utils::json::JSONObject& getExtraData();
    std::string getExtraDataValue(const std::string& zKey) const;

    void setTopBlock(const org::minima::objects::base::MiniNumber& zTopBlock);
    const org::minima::objects::base::MiniNumber& getTopBlock() const;

    const org::minima::objects::base::MiniString& getVersion() const;

    org::minima::objects::base::MiniNumber getRootBlock() const;

    std::vector<std::unique_ptr<org::minima::objects::base::MiniData>>& getChain();
    const std::vector<std::unique_ptr<org::minima::objects::base::MiniData>>& getChain() const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Static helper to read from stream (Java-style)
    static std::shared_ptr<Greeting> ReadFromStream(std::istream& in);

private:
    // What version of Minima
    std::unique_ptr<org::minima::objects::base::MiniString> mVersion;

    // Extra information sent in the greeting
    std::unique_ptr<org::minima::utils::json::JSONObject> mExtraData;

    // The block number of the top block
    std::unique_ptr<org::minima::objects::base::MiniNumber> mTopBlock;

    // The hash chain of the txpow in the current chain - from top down to root of Tree
    std::vector<std::unique_ptr<org::minima::objects::base::MiniData>> mChain;
};

} // namespace objects
} // namespace minima
} // namespace org