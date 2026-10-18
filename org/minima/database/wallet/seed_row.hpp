#pragma once

#include <string>
#include <stdexcept>

#include <sqlite3.h>
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace database {
namespace wallet {

class SeedRow {
public:
    // Public members to match Java's public fields
    std::string mPhrase;
    std::string mSeed;

    // Construct from a SQLite prepared statement row.
    // Looks up columns by name ("phrase", "seed") to mirror Java's ResultSet.getString("...").
    explicit SeedRow(sqlite3_stmt* stmt);

    // Construct from explicit values
    SeedRow(const std::string& zPhrase, const std::string& zSeed);

    // Getters
    std::string getPhrase() const;
    std::string getSeed() const;

    // Convert to JSON object
    org::minima::utils::json::JSONObject toJSON() const;
};

} // namespace wallet
} // namespace database
} // namespace minima
} // namespace org