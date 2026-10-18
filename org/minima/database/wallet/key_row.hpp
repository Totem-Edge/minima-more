#pragma once

#include <string>
#include <stdexcept>
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace database {
namespace wallet {

// Minimal abstract row reader to mirror Java's ResultSet usage.
// Implementations should fetch column values by name and throw on error.
class ResultSetReader {
public:
    virtual ~ResultSetReader() = default;
    virtual int getInt(const std::string& column) = 0;
    virtual std::string getString(const std::string& column) = 0;
};

class KeyRow {
public:
    // Constructor equivalent to Java KeyRow(ResultSet zResults) throws SQLException
    explicit KeyRow(ResultSetReader& zResults);

    // Constructor equivalent to Java KeyRow(int, int, int, int, String, String, String)
    KeyRow(int zSize,
           int zDepth,
           int zUses,
           int zMaxUses,
           const std::string& zModifier,
           const std::string& zPrivate,
           const std::string& zPublic);

    // Getters
    int getSize() const;
    int getDepth() const;
    int getUses() const;
    int getMaxUses() const;

    const std::string& getModifier() const;
    const std::string& getPrivateKey() const;
    const std::string& getPublicKey() const;

    // JSON serialization equivalent to Java toJSON()
    org::minima::utils::json::JSONObject toJSON() const;

private:
    int mSize{0};
    int mDepth{0};

    int mUses{0};
    int mMaxUses{0};

    std::string mModifier;

    std::string mPublicKey;
    std::string mPrivateKey;
};

} // namespace wallet
} // namespace database
} // namespace minima
} // namespace org