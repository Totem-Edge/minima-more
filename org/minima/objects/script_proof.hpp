#pragma once

#include <memory>
#include <string>

#include "org/minima/utils/streamable.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

// Namespaced forward declarations to avoid heavy includes in the header (Pitfall 10)
namespace org { namespace minima { namespace objects { namespace mmr {
    class MMRProof;
} } } }
namespace org { namespace minima { namespace objects {
    class Address;
} } }
namespace org { namespace minima { namespace utils { namespace json {
    class JSONObject;
} } } }

namespace org {
namespace minima {
namespace objects {

class ScriptProof final : public org::minima::utils::Streamable {
public:
    // Constructors matching Java API
    explicit ScriptProof(const std::string& zScript);
    ScriptProof(const std::string& zScript, const org::minima::objects::mmr::MMRProof& zProof);

    // Destructor and special members required due to unique_ptr to forward-declared types (Pitfall 7)
    virtual ~ScriptProof();
    ScriptProof(ScriptProof&&) noexcept;
    ScriptProof& operator=(ScriptProof&&) noexcept;

    // Delete copy operations (unique_ptr members)
    ScriptProof(const ScriptProof& zOther);
    ScriptProof& operator=(const ScriptProof&) = delete;

    // Accessors
    const org::minima::objects::base::MiniString& getScript() const;
    const org::minima::objects::mmr::MMRProof& getProof() const;
    const org::minima::objects::Address& getAddress() const;
    org::minima::objects::base::MiniData getAddressData() const;

    // JSON
    org::minima::utils::json::JSONObject toJSON() const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Static read helper
    static ScriptProof ReadFromStream(std::istream& in);

private:
    // Private default constructor for ReadFromStream (matches Java's private default)
    ScriptProof();

    // Recomputes mAddress from mScript and mProof
    void calculateAddress();

    // Members
    org::minima::objects::base::MiniString mScript;
    std::unique_ptr<org::minima::objects::mmr::MMRProof> mProof;
    std::unique_ptr<org::minima::objects::Address> mAddress;
};

} // namespace objects
} // namespace minima
} // namespace org