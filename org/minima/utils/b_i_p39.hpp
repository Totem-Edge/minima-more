#pragma once

#include <string>
#include <vector>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_string.hpp"

namespace org {
namespace minima {
namespace utils {

class BIP39 {
public:
    // Returns 24 randomly selected words from the BIP39 word list, uppercased and trimmed.
    static std::vector<std::string> getNewWordList();

    // Joins words with spaces and trims the result.
    static std::string convertWordListToString(const std::vector<std::string>& zWords);

    // Hashes the joined word list bytes into MiniData (SHA3-256, matching Java's SHA3Digest(256)).
    static org::minima::objects::base::MiniData convertWordListToSeed(const std::vector<std::string>& zWords);

    // Hashes the phrase bytes into MiniData (SHA3-256, matching Java's SHA3Digest(256)).
    static org::minima::objects::base::MiniData convertStringToSeed(const std::string& zPhrase);

    // Validates and normalizes a seed phrase:
    // - each token must be at least 3 chars
    // - for len < 4: exact match required
    // - for len >= 4: must match the start of some word in the list (first match is chosen)
    // Returns the normalized phrase in uppercase, tokens separated by single spaces.
    // Throws std::invalid_argument on error.
    static std::string cleanSeedPhrase(const std::string& zSeedPhrase);

    // Accessor for the full word list (lowercase words, original order).
    static const std::vector<std::string>& WORD_LIST();
};

} // namespace utils
} // namespace minima
} // namespace org