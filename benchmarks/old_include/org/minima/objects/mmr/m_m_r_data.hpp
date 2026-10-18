#pragma once

#include <memory>
#include <string>
#include <ostream> // For std::ostream
#include <istream> // For std::istream

#include "org/minima/utils/streamable.hpp"

// Namespaced forward declarations
namespace org { namespace minima { namespace objects { namespace base {
    class MiniData;
    class MiniNumber;
} } } }

namespace org { namespace minima { namespace utils {
    namespace json { class JSONObject; }
} } }

namespace org {
namespace minima {
namespace objects {
namespace mmr {

class MMRData : public org::minima::utils::Streamable {
public:
    // Constructors
    MMRData(const org::minima::objects::base::MiniData& zHash,
            const org::minima::objects::base::MiniNumber& zValue);

    virtual ~MMRData();
    MMRData(MMRData&&) noexcept;
    MMRData& operator=(MMRData&&) noexcept;
    
    // NOTE: Keep the copy constructor and assignment operator deleted, 
    // and provide a clone method for deep copies.
    MMRData(const MMRData&) = delete;
    MMRData& operator=(const MMRData&) = delete;
    
    // Helper function to create a deep copy, needed by MMREntry constructor
    std::unique_ptr<MMRData> Clone() const;

    // Static creators
    static std::unique_ptr<MMRData> CreateMMRDataLeafNode(org::minima::utils::Streamable& zData,
                                                          const org::minima::objects::base::MiniNumber& zSumValue);

    static std::unique_ptr<MMRData> CreateMMRDataParentNode(const MMRData& zLeft, const MMRData& zRight);

    // Getters
    const org::minima::objects::base::MiniData& getData() const;
    const org::minima::objects::base::MiniNumber& getValue() const;

    bool mUnspendable = false;

    void setUnspendable(bool zUnspendable);
    bool isUnspendable() const;

    bool isEqual(const MMRData& zData) const;

    org::minima::utils::json::JSONObject toJSON() const;
    std::string toString() const;

    // Streamable
    virtual void writeDataStream(std::ostream& out) override; 
    virtual void readDataStream(std::istream& in) override;

    // Static read helper - MUST return unique_ptr<MMRData>
    static std::unique_ptr<MMRData> ReadFromStream(std::istream& in); 

private:
    MMRData();

    std::unique_ptr<org::minima::objects::base::MiniData> mData;
    std::unique_ptr<org::minima::objects::base::MiniNumber> mValue;
};

} // namespace mmr
} // namespace objects
} // namespace minima
} // namespace org