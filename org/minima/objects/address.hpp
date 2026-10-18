#pragma once

#include <memory>
#include <string>

#include "org/minima/utils/streamable.hpp"

// Namespaced forward declarations (Pitfall 10)
namespace org { namespace minima { namespace objects { namespace base {
    class MiniData;
    class MiniString;
    class MiniNumber;
} } } }

namespace org { namespace minima { namespace utils { namespace json {
    class JSONObject;
} } } }

namespace org {
namespace minima {
namespace objects {

class Address : public org::minima::utils::Streamable {
public:
    // Constructors
    Address();
    explicit Address(const std::string& zScript);
    explicit Address(const org::minima::objects::base::MiniData& zAddressData);

    // Destructor and move operations (PIMPL fix for unique_ptr to forward-declared types)
    virtual ~Address();
    Address(Address&&) noexcept;
    Address& operator=(Address&&) noexcept;

    // Delete copy operations
    Address(const Address& zOther);
    Address& operator=(const Address&) = delete;

    // JSON
    org::minima::utils::json::JSONObject toJSON() const;

    // String
    std::string toString() const;

    // Getters
    std::string getScript() const;
    org::minima::objects::base::MiniData getAddressData() const;
    std::string getMinimaAddress() const;

    // Equality
    bool isEqual(const Address& zAddress) const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Static helpers
    static Address ReadFromStream(std::istream& in);
    static std::string makeMinimaAddress(const org::minima::objects::base::MiniData& zAddress);
    static org::minima::objects::base::MiniData convertMinimaAddress(const std::string& zMinimAddress);

    // Replacement for Java's public static TRUE_ADDRESS
    static const Address& getTrueAddress();

private:
    std::unique_ptr<org::minima::objects::base::MiniString> mScript;
    std::unique_ptr<org::minima::objects::base::MiniData>   mAddressData;
    std::string mMinimaAddress;
};

} // namespace objects
} // namespace minima
} // namespace org