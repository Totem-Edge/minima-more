#pragma once

#include <string>

// Forward declarations to avoid heavy includes in the header (Pitfall 4)
namespace org { namespace minima { namespace utils { namespace json {
class JSONObject;
}}}}

namespace org {
namespace minima {
namespace system {
namespace network {
namespace rpc {

class Authorizer {
public:
    // Replicates: public static JSONObject checkAuchCredentials(String zAuthHeader)
    static org::minima::utils::json::JSONObject checkAuchCredentials(const std::string& zAuthHeader);
};

} // namespace rpc
} // namespace network
} // namespace system
} // namespace minima
} // namespace org