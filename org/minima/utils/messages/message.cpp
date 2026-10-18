#include "org/minima/utils/messages/message.hpp"

#include <sstream>
#include <typeinfo>

#ifdef _WIN32
// No OS-specific behavior required for this class currently.
#else
// No OS-specific behavior required for this class currently.
#endif

namespace org {
namespace minima {
namespace utils {
namespace messages {

namespace {
    // Helper to stringify std::any similar to Java's Object.toString()
    std::string anyToString(const std::any& a) {
        if (!a.has_value()) {
            return "null";
        }

        const std::type_info& ti = a.type();

        try {
            if (ti == typeid(std::string)) {
                return std::any_cast<const std::string&>(a);
            } else if (ti == typeid(const char*)) {
                const char* cstr = std::any_cast<const char*>(a);
                return cstr ? std::string(cstr) : std::string("null");
            } else if (ti == typeid(char*)) {
                const char* cstr = std::any_cast<char*>(a);
                return cstr ? std::string(cstr) : std::string("null");
            } else if (ti == typeid(bool)) {
                return std::any_cast<bool>(a) ? "true" : "false";
            } else if (ti == typeid(int)) {
                return std::to_string(std::any_cast<int>(a));
            } else if (ti == typeid(long)) {
                return std::to_string(std::any_cast<long>(a));
            } else if (ti == typeid(long long)) {
                return std::to_string(std::any_cast<long long>(a));
            } else if (ti == typeid(unsigned int)) {
                return std::to_string(std::any_cast<unsigned int>(a));
            } else if (ti == typeid(unsigned long)) {
                return std::to_string(std::any_cast<unsigned long>(a));
            } else if (ti == typeid(unsigned long long)) {
                return std::to_string(std::any_cast<unsigned long long>(a));
            } else if (ti == typeid(float)) {
                std::ostringstream oss;
                oss << std::any_cast<float>(a);
                return oss.str();
            } else if (ti == typeid(double)) {
                std::ostringstream oss;
                oss << std::any_cast<double>(a);
                return oss.str();
            }
        } catch (const std::bad_any_cast&) {
            // Fall through to generic representation
        }

        // Fallback similar to Java's ClassName@hash-ish; here we just use type name.
        return std::string("<") + ti.name() + ">";
    }
} // anonymous namespace

// Constructors
Message::Message(const std::string& zMessageType)
    : mMessageType(zMessageType),
      mContents(std::make_shared<std::unordered_map<std::string, std::any>>()) {
}

Message::Message()
    : mMessageType(""),
      mContents(std::make_shared<std::unordered_map<std::string, std::any>>()) {
}

// Set/Get message type
void Message::setMessageType(const std::string& zMessageType) {
    mMessageType = zMessageType;
}

const std::string& Message::getMessageType() const {
    return mMessageType;
}

bool Message::isMessageType(const std::string& zMessageType) const {
    return (zMessageType == mMessageType);
}

// Access to all contents (shared to allow aliasing)
std::shared_ptr<std::unordered_map<std::string, std::any>> Message::getAllContents() {
    return mContents;
}

// Adders
Message& Message::addObject(const std::string& zName, const std::any& zObject) {
    // Store the object (may be empty any -> treated as "null" value)
    (*mContents)[zName] = zObject;
    return *this;
}

Message& Message::addObject(const std::string& zName, std::nullptr_t) {
    // Explicitly store a "null" value
    (*mContents)[zName] = std::any{};
    return *this;
}

Message& Message::addFloat(const std::string& zName, float zValue) {
    return addObject(zName, std::any(zValue));
}

Message& Message::addInteger(const std::string& zName, int zValue) {
    return addObject(zName, std::any(zValue));
}

Message& Message::addString(const std::string& zName, const std::string& zValue) {
    // Copy construct a new string (similar to new String(zValue) in Java)
    return addObject(zName, std::any(std::string(zValue)));
}

Message& Message::addBoolean(const std::string& zName, bool zValue) {
    return addObject(zName, std::any(zValue));
}

// Existence check: true only if key exists and value is non-null
bool Message::exists(const std::string& zVariable) const {
    auto it = mContents->find(zVariable);
    if (it == mContents->end()) {
        return false;
    }
    // Present but "null" (empty any) should return false per Java's exists()
    return it->second.has_value();
}

// Getters
std::any Message::getObject(const std::string& zName) const {
    auto it = mContents->find(zName);
    if (it == mContents->end()) {
        return std::any{}; // null
    }
    // Could be empty any (null) or a value
    return it->second;
}

bool Message::getBoolean(const std::string& zName) const {
    auto it = mContents->find(zName);
    if (it == mContents->end()) {
        return false; // like Java: null -> false
    }
    const std::any& val = it->second;
    if (!val.has_value()) {
        return false; // explicit null -> false
    }
    return std::any_cast<bool>(val);
}

bool Message::getBoolean(const std::string& zName, bool zDefault) const {
    if (exists(zName)) {
        return getBoolean(zName);
    }
    return zDefault;
}

int Message::getInteger(const std::string& zName) const {
    auto it = mContents->find(zName);
    if (it == mContents->end()) {
        // Mimic Java: would result in NullPointerException via cast/intValue; here bad_any_cast or undefined -> throw
        throw std::bad_any_cast();
    }
    return std::any_cast<int>(it->second);
}

float Message::getFloat(const std::string& zName) const {
    auto it = mContents->find(zName);
    if (it == mContents->end()) {
        throw std::bad_any_cast();
    }
    return std::any_cast<float>(it->second);
}

std::string Message::getString(const std::string& zName) const {
    auto it = mContents->find(zName);
    if (it == mContents->end() || !it->second.has_value()) {
        // Java returns null; here we return empty string to avoid undefined/null in C++
        return std::string();
    }
    // If the stored type is std::string, cast directly; else try to stringify
    if (it->second.type() == typeid(std::string)) {
        return std::any_cast<const std::string&>(it->second);
    }
    // If some other type was stored, mimic Java cast failure
    return std::any_cast<const std::string&>(it->second);
}

// Stringify
std::string Message::toString() const {
    std::ostringstream oss;
    oss << "[ " << mMessageType << ", ";

    bool first = true;
    for (const auto& kv : *mContents) {
        if (!first) {
            oss << "";
        }
        first = false;

        oss << kv.first << ":" << anyToString(kv.second) << ", ";
    }

    oss << " ]";
    return oss.str();
}

} // namespace messages
} // namespace utils
} // namespace minima
} // namespace org