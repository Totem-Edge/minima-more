#include "org/minima/database/wallet/seed_row.hpp"

#include <cstring> // for std::strcmp

namespace org {
namespace minima {
namespace database {
namespace wallet {

namespace {
inline int findColumnIndexByName(sqlite3_stmt* stmt, const char* name) {
    const int cols = sqlite3_column_count(stmt);
    for (int i = 0; i < cols; ++i) {
        const char* colname = sqlite3_column_name(stmt, i);
        if (colname && std::strcmp(colname, name) == 0) {
            return i;
        }
    }
    return -1;
}

inline std::string getTextColumn(sqlite3_stmt* stmt, int idx) {
    if (idx < 0) {
        return std::string();
    }
    const unsigned char* txt = sqlite3_column_text(stmt, idx);
    if (!txt) {
        // Mirror a benign behavior for NULLs by treating as empty string.
        return std::string();
    }
    return std::string(reinterpret_cast<const char*>(txt));
}
} // anonymous namespace

SeedRow::SeedRow(sqlite3_stmt* stmt) {
    if (!stmt) {
        throw std::runtime_error("SeedRow: null sqlite3_stmt provided");
    }

    // Look up required columns by name to mirror Java's ResultSet behavior.
    const int phraseIdx = findColumnIndexByName(stmt, "phrase");
    const int seedIdx   = findColumnIndexByName(stmt, "seed");

    if (phraseIdx < 0) {
        throw std::runtime_error("SeedRow: column 'phrase' not found in result set");
    }
    if (seedIdx < 0) {
        throw std::runtime_error("SeedRow: column 'seed' not found in result set");
    }

    mPhrase = getTextColumn(stmt, phraseIdx);
    mSeed   = getTextColumn(stmt, seedIdx);
}

SeedRow::SeedRow(const std::string& zPhrase, const std::string& zSeed)
    : mPhrase(zPhrase), mSeed(zSeed) {
}

std::string SeedRow::getPhrase() const {
    return mPhrase;
}

std::string SeedRow::getSeed() const {
    return mSeed;
}

org::minima::utils::json::JSONObject SeedRow::toJSON() const {
    org::minima::utils::json::JSONObject ret;
    ret.put("phrase", mPhrase);
    ret.put("seed", mSeed);
    return ret;
}

} // namespace wallet
} // namespace database
} // namespace minima
} // namespace org