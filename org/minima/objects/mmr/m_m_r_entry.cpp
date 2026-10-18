#include "org/minima/objects/mmr/m_m_r_entry.hpp"

#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/mmr/m_m_r_entry_number.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#include <stdexcept>
#include <utility>
#include <iostream> // Needed for std::ostream / std::istream I/O
#include <ios>      // Needed for std::ios_base::failure

namespace org {
namespace minima {
namespace objects {
namespace mmr {

// Private default constructor
MMREntry::MMREntry()
    : mEntryNumber(),
      mRow(0),
      mMMRData(),
      mIsEmpty(true) {}

// Public constructors
MMREntry::MMREntry(int zRow, const MMREntryNumber& zEntry)
    : mEntryNumber(zEntry),
      mRow(zRow),
      mMMRData(),
      mIsEmpty(true) {}

MMREntry::MMREntry(int zRow, const MMREntryNumber& zEntry, const MMRData& zMMRData)
    : mEntryNumber(zEntry),
      mRow(zRow),
      mMMRData(zMMRData.getData(), zMMRData.getValue()),
      mIsEmpty(false) {}

// Accessors
const MMREntryNumber& MMREntry::getEntryNumber() const {
    return mEntryNumber;
}

int MMREntry::getRow() const {
    return mRow;
}

const MMRData* MMREntry::getMMRData() const {
    return mIsEmpty ? nullptr : &mMMRData; // preserve the null sentinel for empty entries
}

bool MMREntry::isEmpty() const {
    return mIsEmpty;
}

// Position checks
bool MMREntry::checkPosition(const MMREntry& zEntry) const {
    return (zEntry.getRow() == mRow) && zEntry.getEntryNumber().isEqual(mEntryNumber);
}

bool MMREntry::checkPosition(const std::vector<MMREntry>& zMultipleEntries) const {
    for (const MMREntry& entry : zMultipleEntries) {
        if (checkPosition(entry)) {
            return true;
        }
    }
    return false;
}

// JSON
org::minima::utils::json::JSONObject MMREntry::toJSON() const {
    using org::minima::utils::json::JSONObject;
    JSONObject ret;

    ret.put("row", mRow);
    ret.put("entry", mEntryNumber.toString());

    if (mIsEmpty) {
        ret.put("data", nullptr);
    } else {
        ret.put("data", mMMRData.toJSON());
    }

    return ret;
}

std::string MMREntry::toString() const {
    return toJSON().toString();
}

// Utility functions for navigating the MMR
int MMREntry::getParentRow() const {
    return mRow + 1;
}

int MMREntry::getChildRow() const {
    return mRow - 1;
}

bool MMREntry::isLeft() const {
    // Fast path (integer indices): parity is just the low bit - no modulo object.
    return mEntryNumber.isEven();
}

bool MMREntry::isRight() const {
    return !isLeft();
}

MMREntryNumber MMREntry::getLeftSibling() const {
    return mEntryNumber.decrement();
}

MMREntryNumber MMREntry::getRightSibling() const {
    return mEntryNumber.increment();
}

MMREntryNumber MMREntry::getSibling() const {
    if (isLeft()) {
        return getRightSibling();
    } else {
        return getLeftSibling();
    }
}

MMREntryNumber MMREntry::getParentEntry() const {
    return mEntryNumber.div2().floor();
}

MMREntryNumber MMREntry::getLeftChildEntry() const {
    return mEntryNumber.mult2();
}

MMREntryNumber MMREntry::getRightChildEntry() const {
    return getLeftChildEntry().increment();
}

// Stream I/O using standard streams (to match component signatures)
void MMREntry::writeDataStream(std::ostream& out) {
    // 1. Write mRow (primitive int) using binary write
    org::minima::objects::base::MiniNumber row(mRow);
    row.writeDataStream(out);
    if (out.fail()) {
        throw std::ios_base::failure("MMREntry::writeDataStream failed writing mRow");
    }

    // 2. Write Entry number using the std::ostream version of writeDataStream
    mEntryNumber.writeDataStream(out);

    // 3. Write MMR data (empty entries are not serialized - preserve prior behavior)
    if (mIsEmpty) {
        throw std::runtime_error("MMREntry::writeDataStream mMMRData is null - cannot serialize empty entry");
    }
    mMMRData.writeDataStream(out);
}

void MMREntry::readDataStream(std::istream& in) {
    mIsEmpty = false;
    
    // 1. Read mRow (primitive int) using binary read
    org::minima::objects::base::MiniNumber row = org::minima::objects::base::MiniNumber::ReadFromStream(in);
    mRow = row.getAsInt();
    // Check for read failure or unexpected EOF (if not at the end of the file/stream)
    if (in.fail() && !in.eof()) {
        throw std::ios_base::failure("MMREntry::readDataStream failed reading mRow");
    }
    
    // 2. Read Entry number using the std::istream version of ReadFromStream
    mEntryNumber = MMREntryNumber::ReadFromStream(in);

    // 3. Read MMR data (MMRData::ReadFromStream returns a unique_ptr; move into the value member)
    mMMRData = std::move(*MMRData::ReadFromStream(in));
}

// Static helper
MMREntry MMREntry::ReadFromStream(std::istream& in) {
    MMREntry entry;
    entry.readDataStream(in);
    return entry;
}

} // namespace mmr
} // namespace objects
} // namespace minima
} // namespace org