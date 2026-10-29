#pragma once

#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace database {

// Read-only consistency report across the coupled stores:
// archive blocks, cascade, TxPoWTree and (optionally) the MegaMMR.
class StateReport {
public:
    // Fills `out` with flat, script-friendly fields and an `ok` boolean.
    static void build(org::minima::utils::json::JSONObject& out);
};

} // namespace database
} // namespace minima
} // namespace org
