#pragma once

#include <memory>
#include <string>

#include "org/minima/utils/streamable.hpp"

// Namespaced forward declarations for project dependencies (no global fwd decls)
namespace org { namespace minima { namespace objects { namespace base {
class MiniData;
class MiniNumber;
class MiniString;
}}}}

namespace org { namespace minima { namespace utils { namespace json {
class JSONObject;
class JSONArray;
}}}}

namespace org { namespace minima { namespace utils { namespace json { namespace parser {
class JSONParser;
class ParseException;
}}}}}

namespace org {
namespace minima {
namespace utils {

class JsonDB : public org::minima::utils::Streamable {
public:
    JsonDB();
    virtual ~JsonDB();                    // For unique_ptr to incomplete type (PIMPL fix)
    JsonDB(JsonDB&&) noexcept;            // move ctor
    JsonDB& operator=(JsonDB&&) noexcept; // move assign

    // Delete copying
    JsonDB(const JsonDB&) = delete;
    JsonDB& operator=(const JsonDB&) = delete;

    // Access to whole JSON data
    org::minima::utils::json::JSONObject& getAllData();
    const org::minima::utils::json::JSONObject& getAllData() const;

    // Exists
    bool exists(const std::string& zName) const;

    // Boolean
    bool getBoolean(const std::string& zName, bool zDefault) const;
    void setBoolean(const std::string& zName, bool zData);

    // Number (stored as string)
    org::minima::objects::base::MiniNumber getNumber(
        const std::string& zName,
        const org::minima::objects::base::MiniNumber& zDefault) const;
    void setNumber(const std::string& zName, const org::minima::objects::base::MiniNumber& zNumber);

    // HEX Data (stored as string)
    org::minima::objects::base::MiniData getData(
        const std::string& zName,
        const org::minima::objects::base::MiniData& zDefault) const;
    void setData(const std::string& zName, const org::minima::objects::base::MiniData& zData);

    // String
    std::string getString(const std::string& zName) const; // returns empty if absent
    std::string getString(const std::string& zName, const std::string& zDefault) const;
    void setString(const std::string& zName, const std::string& zData);

    // JSONObject
    void setJSON(const std::string& zName, const org::minima::utils::json::JSONObject& zJSON);
    org::minima::utils::json::JSONObject getJSON(
        const std::string& zName,
        const org::minima::utils::json::JSONObject& zDefault) const;

    // JSONArray
    void setJSONArray(const std::string& zName, const org::minima::utils::json::JSONArray& zJSONArray);
    org::minima::utils::json::JSONArray getJSONArray(const std::string& zName);

    // Load and Save (filesystem path)
    void loadDB(const std::string& filePath);
    void saveDB(const std::string& filePath);

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

private:
    std::unique_ptr<org::minima::utils::json::JSONObject> mParams;
};

} // namespace utils
} // namespace minima
} // namespace org