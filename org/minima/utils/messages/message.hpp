#pragma once

#include <string>
#include <unordered_map>
#include <any>
#include <memory>

namespace org {
namespace minima {
namespace utils {
namespace messages {

class Message {
public:
    // Constructors
    Message(const std::string& zMessageType);
    Message();

    // Set/Get message type
    void setMessageType(const std::string& zMessageType);
    const std::string& getMessageType() const;
    bool isMessageType(const std::string& zMessageType) const;

    // Access to all contents (shared to allow aliasing like Java)
    std::shared_ptr<std::unordered_map<std::string, std::any>> getAllContents();

    // Adders - return this for chaining
    Message& addObject(const std::string& zName, const std::any& zObject);
    Message& addObject(const std::string& zName, std::nullptr_t); // store a "null" value
    Message& addFloat(const std::string& zName, float zValue);
    Message& addInteger(const std::string& zName, int zValue);
    Message& addString(const std::string& zName, const std::string& zValue);
    Message& addBoolean(const std::string& zName, bool zValue);

    // Existence check (true only if key exists and value is non-null)
    bool exists(const std::string& zVariable) const;

    // Getters
    std::any getObject(const std::string& zName) const;

    bool getBoolean(const std::string& zName) const;
    bool getBoolean(const std::string& zName, bool zDefault) const;

    int   getInteger(const std::string& zName) const;
    float getFloat(const std::string& zName) const;
    // Note: returns empty string when value is null or missing (Java returns null).
    std::string getString(const std::string& zName) const;

    // Stringify
    std::string toString() const;

protected:
    std::string mMessageType;
    // Shared map to emulate Java reference semantics (aliasing when assigned)
    std::shared_ptr<std::unordered_map<std::string, std::any>> mContents;
};

} // namespace messages
} // namespace utils
} // namespace minima
} // namespace org