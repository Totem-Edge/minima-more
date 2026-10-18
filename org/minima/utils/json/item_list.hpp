#pragma once

#include <string>
#include <vector>

namespace org {
namespace minima {
namespace utils {
namespace json {

class ItemList {
public:
    // Constructors
    ItemList();
    explicit ItemList(const std::string& s);
    ItemList(const std::string& s, const std::string& sp);
    ItemList(const std::string& s, const std::string& sp, bool isMultiToken);

    // Access to internal items (mutable and const to mirror Java exposing the list)
    std::vector<std::string>& getItems();
    const std::vector<std::string>& getItems() const;

    // Returns a copy of the items as an array-like container
    std::vector<std::string> getArray() const;

    // Split functions (public, as in Java)
    void split(const std::string& s, const std::string& sp, std::vector<std::string>& append, bool isMultiToken);
    void split(const std::string& s, const std::string& sp, std::vector<std::string>& append);

    // Set separator
    void setSP(const std::string& sp);

    // Add operations
    void add(int i, const std::string& item);
    void add(const std::string& item);

    void addAll(ItemList& list);
    void addAll(const std::string& s);
    void addAll(const std::string& s, const std::string& sp);
    void addAll(const std::string& s, const std::string& sp, bool isMultiToken);

    // Getters
    std::string get(int i) const;
    int size() const;

    // String conversion
    std::string toString() const;
    std::string toString(const std::string& sp) const;

    // Maintenance
    void clear();
    void reset();

private:
    std::string sp_ = ",";
    std::vector<std::string> items_;

    static std::string trimString(const std::string& in);
    static bool isDelimiterChar(char c, const std::string& delims);
};

} // namespace json
} // namespace utils
} // namespace minima
} // namespace org