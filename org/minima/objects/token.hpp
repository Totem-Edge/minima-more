#pragma once

#include <memory>
#include <iosfwd> // std::istream, std::ostream

#include "org/minima/utils/streamable.hpp"

// Namespaced forward declarations (PITFALL 4)
namespace org { namespace minima { namespace objects { namespace base {
    class MiniData;
    class MiniNumber;
    class MiniString;
} } } }

namespace org { namespace minima { namespace utils { namespace json {
    class JSONObject;
} } } }

// Token class
namespace org {
namespace minima {
namespace objects {

class Token : public org::minima::utils::Streamable {
public:
    // Static constants (defined in .cpp)
    static const org::minima::objects::base::MiniData TOKENID_CREATE;
    static const org::minima::objects::base::MiniData TOKENID_MINIMA;

    // Destructor and move operations (PITFALL 1)
    virtual ~Token();
    Token(Token&&) noexcept;
    Token& operator=(Token&&) noexcept;

    // Delete copy operations
    Token(const Token& zOther);
    Token& operator=(const Token&) = delete;

    // Public constructors mirroring Java
    Token(const org::minima::objects::base::MiniData& zCoindID,
          const org::minima::objects::base::MiniNumber& zScale,
          const org::minima::objects::base::MiniNumber& zMinimaAmount,
          const org::minima::objects::base::MiniString& zName,
          const org::minima::objects::base::MiniString& zTokenScript);

    Token(const org::minima::objects::base::MiniData& zCoindID,
          const org::minima::objects::base::MiniNumber& zScale,
          const org::minima::objects::base::MiniNumber& zMinimaAmount,
          const org::minima::objects::base::MiniString& zName,
          const org::minima::objects::base::MiniString& zTokenScript,
          const org::minima::objects::base::MiniNumber& zCreated);

    // Methods (signatures use references/pointers per SPECIAL INSTRUCTIONS)
    std::unique_ptr<org::minima::objects::base::MiniNumber>
    getScaledTokenAmount(const org::minima::objects::base::MiniNumber& zMinimaAmount) const;

    std::unique_ptr<org::minima::objects::base::MiniNumber>
    getScaledMinimaAmount(const org::minima::objects::base::MiniNumber& zTokenAmount) const;

    const org::minima::objects::base::MiniNumber& getScale() const;
    const org::minima::objects::base::MiniNumber& getAmount() const;

    std::unique_ptr<org::minima::objects::base::MiniNumber> getTotalTokens() const;

    std::unique_ptr<org::minima::objects::base::MiniNumber> getDecimalPlaces() const;

    const org::minima::objects::base::MiniString& getName() const;
    const org::minima::objects::base::MiniString& getTokenScript() const;

    const org::minima::objects::base::MiniData& getCoinID() const;

    const org::minima::objects::base::MiniNumber& getCreated() const;

    // May be null (nullptr) if not calculated yet
    const org::minima::objects::base::MiniData* getTokenID() const;

    // JSON representation (returned as owning pointer)
    std::unique_ptr<org::minima::utils::json::JSONObject> toJSON() const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Static helpers
    static std::unique_ptr<Token> convertMiniDataVersion(const org::minima::objects::base::MiniData& zTxpData);
    static std::unique_ptr<Token> ReadFromStream(std::istream& in);

protected:
    // Blank constructor for ReadFromStream
    Token();

private:
    // Members (as pointers due to forward declarations and cycle constraints)
    std::unique_ptr<org::minima::objects::base::MiniData>   mCoinID;
    std::unique_ptr<org::minima::objects::base::MiniNumber> mTokenScale;
    std::unique_ptr<org::minima::objects::base::MiniNumber> mTokenMinimaAmount;
    std::unique_ptr<org::minima::objects::base::MiniString> mTokenName;
    std::unique_ptr<org::minima::objects::base::MiniString> mTokenScript;
    std::unique_ptr<org::minima::objects::base::MiniNumber> mTokenCreated;
    std::unique_ptr<org::minima::objects::base::MiniData>   mTokenID;

    void calculateTokenID();
};

} // namespace objects
} // namespace minima
} // namespace org