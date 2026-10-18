#pragma once

#include <string>

namespace org {
namespace minima {
namespace utils {
namespace json {

class JSONAware {
public:
    virtual ~JSONAware();

    // Returns JSON text
    virtual std::string toJSONString() const = 0;
};

} // namespace json
} // namespace utils
} // namespace minima
} // namespace org