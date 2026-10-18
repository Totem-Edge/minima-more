#pragma once

#include <memory>
#include <string>
#include <vector>
#include <istream>
#include <ostream>

#include "org/minima/utils/streamable.hpp"

// FIX: Ensure these includes are present to fix the PIMPL bug.
#include "org/minima/objects/mmr/m_m_r_entry_number.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"

// Forward declarations
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace objects {
namespace mmr {

class MMREntry : public org::minima::utils::Streamable {
private:
    // Stored by value (was unique_ptr): removes two heap allocations per entry.
    // "Empty" is tracked by mIsEmpty; the previous null-sentinel is preserved by
    // getMMRData() returning nullptr when mIsEmpty.
    org::minima::objects::mmr::MMREntryNumber mEntryNumber;
    int mRow { 0 };
    org::minima::objects::mmr::MMRData mMMRData;
    bool mIsEmpty { true };

public:
    MMREntry(); // default (empty); public so MMREntry can be a map value type
    MMREntry(int zRow, const org::minima::objects::mmr::MMREntryNumber& zEntry);
    MMREntry(int zRow, const org::minima::objects::mmr::MMREntryNumber& zEntry,
             const org::minima::objects::mmr::MMRData& zMMRData);

    virtual ~MMREntry() = default; // Default destructor in header requires full defs above
    MMREntry(MMREntry&&) noexcept = default;
    MMREntry& operator=(MMREntry&&) noexcept = default;

    // Copy Constructor (inline) - MMRData has no copy ctor, so reconstruct it.
    MMREntry(const MMREntry& other)
        : mEntryNumber(other.mEntryNumber),
          mRow(other.mRow),
          mMMRData(other.mMMRData.getData(), other.mMMRData.getValue()),
          mIsEmpty(other.mIsEmpty)
    {}

    // Copy Assignment (inline)
    MMREntry& operator=(const MMREntry& other) {
        if (this == &other) return *this;
        mEntryNumber = other.mEntryNumber;
        mRow = other.mRow;
        mMMRData = org::minima::objects::mmr::MMRData(other.mMMRData.getData(), other.mMMRData.getValue());
        mIsEmpty = other.mIsEmpty;
        return *this;
    }

    const org::minima::objects::mmr::MMREntryNumber& getEntryNumber() const;
    int getRow() const;
    const org::minima::objects::mmr::MMRData* getMMRData() const;
    
    // ADDED Non-Const GetMMRData
    org::minima::objects::mmr::MMRData* getMMRData() { return mIsEmpty ? nullptr : &mMMRData; }

    bool isEmpty() const;
    bool checkPosition(const MMREntry& zEntry) const;
    
    // CHANGED Signature
    bool checkPosition(const std::vector<MMREntry>& zMultipleEntries) const;

    org::minima::utils::json::JSONObject toJSON() const;
    std::string toString() const;
    int getParentRow() const;
    int getChildRow() const;
    bool isLeft() const;
    bool isRight() const;
    org::minima::objects::mmr::MMREntryNumber getLeftSibling() const;
    org::minima::objects::mmr::MMREntryNumber getRightSibling() const;
    org::minima::objects::mmr::MMREntryNumber getSibling() const;
    org::minima::objects::mmr::MMREntryNumber getParentEntry() const;
    org::minima::objects::mmr::MMREntryNumber getLeftChildEntry() const;
    org::minima::objects::mmr::MMREntryNumber getRightChildEntry() const;
    virtual void writeDataStream(std::ostream& out) override;
    virtual void readDataStream(std::istream& in) override;
    static MMREntry ReadFromStream(std::istream& in);
};

} // namespace mmr
} // namespace objects
} // namespace minima
} // namespace org