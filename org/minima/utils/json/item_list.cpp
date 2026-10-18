#include "org/minima/utils/json/item_list.hpp"

#include <stdexcept>
#include <sstream>
#include <algorithm>

namespace org {
namespace minima {
namespace utils {
namespace json {

// Helper: mimic Java String.trim() (remove chars with code <= 0x20 from both ends)
std::string ItemList::trimString(const std::string& in) {
    size_t start = 0;
    while (start < in.size() && static_cast<unsigned char>(in[start]) <= 0x20) {
        ++start;
    }
    size_t end = in.size();
    while (end > start && static_cast<unsigned char>(in[end - 1]) <= 0x20) {
        --end;
    }
    return in.substr(start, end - start);
}

bool ItemList::isDelimiterChar(char c, const std::string& delims) {
    // StringTokenizer uses any char in delims as a delimiter.
    return delims.find(c) != std::string::npos;
}

// Constructors
ItemList::ItemList() = default;

ItemList::ItemList(const std::string& s) {
    split(s, sp_, items_);
}

ItemList::ItemList(const std::string& s, const std::string& sp) {
    // Replicate Java behavior: this.sp = s (likely a bug in Java, preserved for equivalence)
    sp_ = s;
    split(s, sp, items_);
}

ItemList::ItemList(const std::string& s, const std::string& sp, bool isMultiToken) {
    split(s, sp, items_, isMultiToken);
}

// Accessors for items
std::vector<std::string>& ItemList::getItems() {
    return items_;
}

const std::vector<std::string>& ItemList::getItems() const {
    return items_;
}

// Return copy as array-like container
std::vector<std::string> ItemList::getArray() const {
    return items_;
}

// Split functions
void ItemList::split(const std::string& s, const std::string& sp, std::vector<std::string>& append, bool isMultiToken) {
    if (isMultiToken) {
        // Emulate Java StringTokenizer(s, sp): split on any delimiter char, collapse runs, no empty tokens.
        const std::size_t n = s.size();
        std::size_t i = 0;
        while (i < n) {
            // Skip leading delimiters
            while (i < n && isDelimiterChar(s[i], sp)) {
                ++i;
            }
            if (i >= n) {
                break;
            }
            // Find end of token
            std::size_t j = i;
            while (j < n && !isDelimiterChar(s[j], sp)) {
                ++j;
            }
            std::string token = trimString(s.substr(i, j - i));
            append.push_back(token);
            i = j;
        }
    } else {
        split(s, sp, append);
    }
}

void ItemList::split(const std::string& s, const std::string& sp, std::vector<std::string>& append) {
    // Mirror Java logic with indexOf loop, preserving empty tokens and boundaries.
    std::size_t pos = 0;
    std::size_t prevPos = 0;

    // Use a do-while style as in Java
    for (;;) {
        prevPos = pos;
        std::size_t found = s.find(sp, pos);
        if (found == std::string::npos) {
            break;
        }
        append.push_back(trimString(s.substr(prevPos, found - prevPos)));
        pos = found + sp.size();
        // Continue until no more found
    }
    append.push_back(trimString(s.substr(prevPos)));
}

// Set separator
void ItemList::setSP(const std::string& sp) {
    sp_ = sp;
}

// Add operations
void ItemList::add(int i, const std::string& item) {
    // In Java, null items are ignored; in C++, std::string cannot be null, so we always add.
    if (i < 0 || static_cast<std::size_t>(i) > items_.size()) {
        throw std::out_of_range("Index: " + std::to_string(i) + ", Size: " + std::to_string(items_.size()));
    }
    items_.insert(items_.begin() + static_cast<std::size_t>(i), trimString(item));
}

void ItemList::add(const std::string& item) {
    // In Java, null items are ignored; here we always add the provided string.
    items_.push_back(trimString(item));
}

void ItemList::addAll(ItemList& list) {
    items_.insert(items_.end(), list.items_.begin(), list.items_.end());
}

void ItemList::addAll(const std::string& s) {
    split(s, sp_, items_);
}

void ItemList::addAll(const std::string& s, const std::string& sp) {
    split(s, sp, items_);
}

void ItemList::addAll(const std::string& s, const std::string& sp, bool isMultiToken) {
    split(s, sp, items_, isMultiToken);
}

// Getters
std::string ItemList::get(int i) const {
    if (i < 0 || static_cast<std::size_t>(i) >= items_.size()) {
        throw std::out_of_range("Index: " + std::to_string(i) + ", Size: " + std::to_string(items_.size()));
    }
    return items_[static_cast<std::size_t>(i)];
}

int ItemList::size() const {
    return static_cast<int>(items_.size());
}

// toString
std::string ItemList::toString() const {
    return toString(sp_);
}

std::string ItemList::toString(const std::string& sp) const {
    std::string result;
    if (items_.empty()) {
        return result;
    }
    // Precompute approximate size? Not necessary; keep simple and correct.
    for (std::size_t i = 0; i < items_.size(); ++i) {
        if (i != 0) {
            result += sp;
        }
        result += items_[i];
    }
    return result;
}

// Maintenance
void ItemList::clear() {
    items_.clear();
}

void ItemList::reset() {
    sp_ = ",";
    items_.clear();
}

} // namespace json
} // namespace utils
} // namespace minima
} // namespace org