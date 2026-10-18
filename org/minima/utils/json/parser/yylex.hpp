#pragma once

#include <memory>
#include <istream>
#include <string>
#include <vector>
#include <cstdint> // FIX 1: Added missing include for uint16_t

namespace org {
namespace minima {
namespace utils {
namespace json {
namespace parser {

class Yytoken;        // forward declaration due to cycle
class ParseException; // forward declaration due to cycle

class Yylex {
public:
    // Constants
    static constexpr int YYEOF = -1;
    static constexpr int YYINITIAL = 0;
    static constexpr int STRING_BEGIN = 2;

    // Constructors
    explicit Yylex(std::istream& in);                       // non-owning
    explicit Yylex(std::unique_ptr<std::istream> in_own);   // owning
    ~Yylex() = default;

    // Public API (mirrors Java)
    int getPosition() const;

    void yyclose();
    void yyreset(std::istream& reader);

    int yystate() const;
    void yybegin(int newState);

    std::string yytext() const;
    char yycharat(int pos) const;
    int yylength() const;

    void yypushback(int number);

    // returns nullptr on EOF
    std::unique_ptr<Yytoken> yylex();

private:
    // Error handling
    void zzScanError(int errorCode);

    // Internals
    // uint16_t is now defined
    static std::vector<uint16_t> zzUnpackCMap(const std::u16string& packed);
    static std::vector<int> zzUnpackAction(const std::u16string& packed);
    static std::vector<int> zzUnpackRowMap(const std::u16string& packed);
    static std::vector<int> zzUnpackAttribute(const std::u16string& packed);

    bool zzRefill();

private:
    // Generated tables/constants
    static const int ZZ_BUFFERSIZE = 16384;

    static const int ZZ_LEXSTATE[4];

    static const std::u16string ZZ_CMAP_PACKED;
    // uint16_t is now defined
    static const std::vector<uint16_t> ZZ_CMAP;

    static const std::u16string ZZ_ACTION_PACKED_0;
    static const std::vector<int> ZZ_ACTION;

    static const std::u16string ZZ_ROWMAP_PACKED_0;
    static const std::vector<int> ZZ_ROWMAP;

    static const int ZZ_TRANS[];

    static const int ZZ_UNKNOWN_ERROR = 0;
    static const int ZZ_NO_MATCH = 1;
    static const int ZZ_PUSHBACK_2BIG = 2;

    static const char* const ZZ_ERROR_MSG[3];

    static const std::u16string ZZ_ATTRIBUTE_PACKED_0;
    static const std::vector<int> ZZ_ATTRIBUTE;

private:
    // State
    std::unique_ptr<std::istream> m_owned;
    std::istream* zzReader;

    int zzState = 0;
    int zzLexicalState = YYINITIAL;

    std::vector<char> zzBuffer; // byte buffer
    int zzMarkedPos = 0;
    int zzCurrentPos = 0;
    int zzStartRead = 0;
    int zzEndRead = 0;

    int yyline = 0;
    int yychar = 0;
    int yycolumn = 0;

    bool zzAtBOL = true;
    bool zzAtEOF = false;

    // user buffer
    std::string sb;
};

} // namespace parser
} // namespace json
} // namespace utils
} // namespace minima
} // namespace org
