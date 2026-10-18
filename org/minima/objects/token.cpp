#include "org/minima/objects/token.hpp"

#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <exception>
#include <algorithm>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/utils/crypto.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#ifdef _WIN32
// No OS-specific behavior required here, placeholder in case of future differences
#endif

namespace org {
namespace minima {
namespace objects {

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniString;
using org::minima::utils::Crypto;
using org::minima::utils::MinimaLogger;
using org::minima::utils::json::JSONObject;

// Static constants
const MiniData Token::TOKENID_CREATE = MiniData("0xFF");
const MiniData Token::TOKENID_MINIMA = MiniData("0x00");

// Destructor and move ops
Token::~Token() = default;
Token::Token(Token&&) noexcept = default;
Token& Token::operator=(Token&&) noexcept = default;

// Protected blank constructor
Token::Token() = default;

// Public constructors
Token::Token(const MiniData& zCoindID,
             const MiniNumber& zScale,
             const MiniNumber& zMinimaAmount,
             const MiniString& zName,
             const MiniString& zTokenScript)
{
    mCoinID             = std::make_unique<MiniData>(zCoindID);
    mTokenName          = std::make_unique<MiniString>(zName);
    mTokenScale         = std::make_unique<MiniNumber>(zScale);
    mTokenMinimaAmount  = std::make_unique<MiniNumber>(zMinimaAmount);
    mTokenScript        = std::make_unique<MiniString>(zTokenScript.toString());
    mTokenCreated       = std::make_unique<MiniNumber>(MiniNumber::ZERO());
    // mTokenID calculated
    calculateTokenID();
}

Token::Token(const MiniData& zCoindID,
             const MiniNumber& zScale,
             const MiniNumber& zMinimaAmount,
             const MiniString& zName,
             const MiniString& zTokenScript,
             const MiniNumber& zCreated)
{
    mCoinID             = std::make_unique<MiniData>(zCoindID);
    mTokenName          = std::make_unique<MiniString>(zName);
    mTokenScale         = std::make_unique<MiniNumber>(zScale);
    mTokenMinimaAmount  = std::make_unique<MiniNumber>(zMinimaAmount);
    mTokenScript        = std::make_unique<MiniString>(zTokenScript.toString());
    mTokenCreated       = std::make_unique<MiniNumber>(zCreated);
    // mTokenID calculated
    calculateTokenID();
}

Token::Token(const Token& zOther)
{
    // Manually copy each unique_ptr by creating a new one
    // from the contents of the old one.
    // This assumes MiniData, MiniNumber, and MiniString are copyable.

    if (zOther.mCoinID) {
        mCoinID = std::make_unique<org::minima::objects::base::MiniData>(*zOther.mCoinID);
    }
    if (zOther.mTokenScale) {
        mTokenScale = std::make_unique<org::minima::objects::base::MiniNumber>(*zOther.mTokenScale);
    }
    if (zOther.mTokenMinimaAmount) {
        mTokenMinimaAmount = std::make_unique<org::minima::objects::base::MiniNumber>(*zOther.mTokenMinimaAmount);
    }
    if (zOther.mTokenName) {
        mTokenName = std::make_unique<org::minima::objects::base::MiniString>(*zOther.mTokenName);
    }
    if (zOther.mTokenScript) {
        mTokenScript = std::make_unique<org::minima::objects::base::MiniString>(*zOther.mTokenScript);
    }
    if (zOther.mTokenCreated) {
        mTokenCreated = std::make_unique<org::minima::objects::base::MiniNumber>(*zOther.mTokenCreated);
    }

    // The mTokenID is calculated, but we should copy it if it exists
    if (zOther.mTokenID) {
        mTokenID = std::make_unique<org::minima::objects::base::MiniData>(*zOther.mTokenID);
    }
}

// Scaling helpers
std::unique_ptr<MiniNumber>
Token::getScaledTokenAmount(const MiniNumber& zMinimaAmount) const {
    if (!mTokenScale) {
        throw std::runtime_error("Token::getScaledTokenAmount - mTokenScale is null");
    }
    int scale = mTokenScale->getAsInt();
    MiniNumber current = zMinimaAmount;
    for (int i = 0; i < scale; ++i) {
        current = current.mult(MiniNumber::TEN());
    }
    return std::make_unique<MiniNumber>(current);
}

std::unique_ptr<MiniNumber>
Token::getScaledMinimaAmount(const MiniNumber& zTokenAmount) const {
    if (!mTokenScale) {
        throw std::runtime_error("Token::getScaledMinimaAmount - mTokenScale is null");
    }
    int scale = mTokenScale->getAsInt();
    MiniNumber current = zTokenAmount;
    for (int i = 0; i < scale; ++i) {
        current = current.div(MiniNumber::TEN());
    }
    return std::make_unique<MiniNumber>(current);
}

// Getters
const MiniNumber& Token::getScale() const {
    if (!mTokenScale) throw std::runtime_error("Token::getScale - null");
    return *mTokenScale;
}

const MiniNumber& Token::getAmount() const {
    if (!mTokenMinimaAmount) throw std::runtime_error("Token::getAmount - null");
    return *mTokenMinimaAmount;
}

std::unique_ptr<MiniNumber> Token::getTotalTokens() const {
    if (!mTokenMinimaAmount) throw std::runtime_error("Token::getTotalTokens - null amount");
    return getScaledTokenAmount(*mTokenMinimaAmount);
}

std::unique_ptr<MiniNumber> Token::getDecimalPlaces() const {
    if (!mTokenScale) throw std::runtime_error("Token::getDecimalPlaces - null");
    int dec = MiniNumber::MAX_DECIMAL_PLACES - mTokenScale->getAsInt();
    return std::make_unique<MiniNumber>(dec);
}

const MiniString& Token::getName() const {
    if (!mTokenName) throw std::runtime_error("Token::getName - null");
    return *mTokenName;
}

const MiniString& Token::getTokenScript() const {
    if (!mTokenScript) throw std::runtime_error("Token::getTokenScript - null");
    return *mTokenScript;
}

const MiniData& Token::getCoinID() const {
    if (!mCoinID) throw std::runtime_error("Token::getCoinID - null");
    return *mCoinID;
}

const MiniNumber& Token::getCreated() const {
    if (!mTokenCreated) throw std::runtime_error("Token::getCreated - null");
    return *mTokenCreated;
}

const MiniData* Token::getTokenID() const {
    return mTokenID ? mTokenID.get() : nullptr;
}

// Helper: trim string
static std::string trim_copy(const std::string& s) {
    auto start = s.begin();
    auto end   = s.end();
    while (start != end && std::isspace(static_cast<unsigned char>(*start))) ++start;
    while (end != start && std::isspace(static_cast<unsigned char>(*(end - 1)))) --end;
    return std::string(start, end);
}

// JSON
std::unique_ptr<JSONObject> Token::toJSON() const {
    auto obj = std::make_unique<JSONObject>();

    // Is Token Name a JSON (we still store as string for safe JSON serialization)
    std::string nameStr = mTokenName ? mTokenName->toString() : std::string();
    std::string trimmed = trim_copy(nameStr);
    (void)trimmed; // Decision preserved, but string stored either way
    obj->put("name", nameStr);

    // Other fields (strings for deterministic output)
    obj->put("coinid", mCoinID ? mCoinID->to0xString() : std::string());
    auto tot = getTotalTokens();
    obj->put("total", tot ? tot->toString() : std::string());

    auto dec = getDecimalPlaces();
    obj->put("decimals", dec ? dec->getAsInt() : 0);

    obj->put("script", mTokenScript ? mTokenScript->toString() : std::string());
    obj->put("totalamount", mTokenMinimaAmount ? mTokenMinimaAmount->toString() : std::string());
    obj->put("scale", mTokenScale ? mTokenScale->toString() : std::string());
    obj->put("created", mTokenCreated ? mTokenCreated->toString() : std::string());

    if (!mTokenID) {
        obj->put("tokenid", std::any{}); // null
    } else {
        obj->put("tokenid", mTokenID->to0xString());
    }

    return obj;
}

// calculateTokenID: serialize Token -> MiniData -> hashObject
void Token::calculateTokenID() {
    try {
        std::ostringstream oss(std::ios::binary);
        writeDataStream(oss);
        const std::string bytes = oss.str();
        std::vector<std::uint8_t> vec(bytes.begin(), bytes.end());

        MiniData tokdat(vec);
        MiniData hashed = Crypto::getInstance().hashObject(tokdat);
        mTokenID = std::make_unique<MiniData>(hashed);
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        mTokenID.reset();
    }
}

// Streamable
void Token::writeDataStream(std::ostream& out) {
    if (!mCoinID || !mTokenScript || !mTokenScale || !mTokenMinimaAmount || !mTokenName || !mTokenCreated) {
        throw std::runtime_error("Token::writeDataStream - one or more fields are null");
    }
    mCoinID->writeHashToStream(out);
    mTokenScript->writeDataStream(out);
    mTokenScale->writeDataStream(out);
    mTokenMinimaAmount->writeDataStream(out);
    mTokenName->writeDataStream(out);
    mTokenCreated->writeDataStream(out);
}

void Token::readDataStream(std::istream& in) {
    mCoinID            = std::make_unique<MiniData>(MiniData::ReadHashFromStream(in));
    mTokenScript       = std::make_unique<MiniString>(MiniString::ReadFromStream(in));
    mTokenScale        = std::make_unique<MiniNumber>(MiniNumber::ReadFromStream(in));
    mTokenMinimaAmount = std::make_unique<MiniNumber>(MiniNumber::ReadFromStream(in));
    mTokenName         = std::make_unique<MiniString>(MiniString::ReadFromStream(in));
    mTokenCreated      = std::make_unique<MiniNumber>(MiniNumber::ReadFromStream(in));

    calculateTokenID();
}

// Static helpers
std::unique_ptr<Token> Token::convertMiniDataVersion(const MiniData& zTxpData) {
    std::unique_ptr<Token> tok;
    try {
        const std::vector<std::uint8_t>& bytes = zTxpData.getBytes();
        std::string dat(bytes.begin(), bytes.end());
        std::istringstream iss(std::string(dat.data(), dat.size()), std::ios::binary);

        tok = Token::ReadFromStream(iss);
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        tok.reset();
    }
    return tok;
}

std::unique_ptr<Token> Token::ReadFromStream(std::istream& in) {
    auto td = std::unique_ptr<Token>(new Token());
    td->readDataStream(in);
    return td;
}

} // namespace objects
} // namespace minima
} // namespace org