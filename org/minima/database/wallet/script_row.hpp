#pragma once

#include <string>

// We must include the full JSONObject definition because toJSON returns it by value.
#include "org/minima/utils/json/j_s_o_n_object.hpp"

// Minimal DB-agnostic ResultSet interface to mirror the subset of java.sql.ResultSet used here.
// Any database layer can implement this interface and provide instances to ScriptRow.
class ResultSet {
public:
    virtual ~ResultSet() = default;

    // Retrieve a column as a string by column name.
    virtual std::string getString(const std::string& column) = 0;

    // Retrieve a column as an int by column name.
    virtual int getInt(const std::string& column) = 0;
};

class ScriptRow {
public:
    // Construct from a DB row result (analogous to java.sql.ResultSet-based constructor).
    explicit ScriptRow(ResultSet& zResults);

    // Construct directly from explicit values.
    ScriptRow(const std::string& zScript,
              const std::string& zAddress,
              bool zSimple,
              bool zDefault,
              const std::string& zPublicKey,
              bool zTrack);

    // Accessors mirroring Java methods.
    std::string getScript() const;
    std::string getAddress() const;
    bool isSimple() const;
    bool isDefault() const;
    std::string getPublicKey() const;
    bool isTrack() const;

    // Convert to JSON, same fields as Java version.
    org::minima::utils::json::JSONObject toJSON() const;

private:
    std::string mScript;
    std::string mAddress;
    bool        mSimple {false};
    bool        mDefault {false};
    std::string mPublicKey;
    bool        mTrack {false};
};