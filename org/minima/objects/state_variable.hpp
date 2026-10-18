#pragma once

#include <string>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <memory>

#include "org/minima/utils/streamable.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/objects/base/mini_byte.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_string.hpp"

namespace org {
namespace minima {
namespace objects {

class StateVariable : public org::minima::utils::Streamable {
public:
    // All possible state variable types (Java: public static final)
    static const org::minima::objects::base::MiniByte STATETYPE_HEX;    // 1
    static const org::minima::objects::base::MiniByte STATETYPE_NUMBER; // 2
    static const org::minima::objects::base::MiniByte STATETYPE_STRING; // 4
    static const org::minima::objects::base::MiniByte STATETYPE_BOOL;   // 8

    // Construct from port and data string
    StateVariable(int zPort, const std::string& zData);

    // Getters
    int getPort() const;
    const org::minima::objects::base::MiniByte& getType() const;
    const org::minima::objects::base::MiniString& getData() const;

    // JSON
    org::minima::utils::json::JSONObject toJSON() const;

    // String
    std::string toString() const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Static read helper (Java: ReadFromStream)
    static std::unique_ptr<StateVariable> ReadFromStream(std::istream& in);

private:
    // Java has a private default constructor
    StateVariable();

    // Helpers
    static std::string trim(const std::string& s);
    static std::string toLower(const std::string& s);
    static bool iequals(const std::string& a, const std::string& b);
    static bool startsWithIgnoreCase(const std::string& s, const std::string& prefix);

private:
    org::minima::objects::base::MiniByte   mType;
    org::minima::objects::base::MiniByte   mPort;
    org::minima::objects::base::MiniString mData;
};

} // namespace objects
} // namespace minima
} // namespace org