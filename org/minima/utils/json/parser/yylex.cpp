#include "org/minima/utils/json/parser/yylex.hpp"

#include <stdexcept>
#include <any> // Still needed for ParseException constructor potentially
#include <cstdint>
#include <cstring>
#include <limits>
#include <sstream>
#include <variant> // Include for std::variant (Yytoken::Value)
#include <iostream>

#include "org/minima/utils/json/parser/yytoken.hpp"
#include "org/minima/utils/json/parser/parse_exception.hpp"

#ifdef _WIN32
// No OS-specific functionality required here, placeholder in case of future needs.
#else
// POSIX: nothing special needed
#endif

namespace org {
namespace minima {
namespace utils {
namespace json {
namespace parser {

// Static generated data (definitions match header now)

const int Yylex::ZZ_LEXSTATE[4] = { 0, 0, 1, 1 };

namespace {
template <std::size_t N>
std::u16string u16_lit(const char16_t (&a)[N]) { return std::u16string(a, N - 1); }
}

const std::u16string Yylex::ZZ_CMAP_PACKED = u16_lit(u"\11\0\1\7\1\7\2\0\1\7\22\0\1\7\1\0\1\11\10\0"
    u"\1\6\1\31\1\2\1\4\1\12\12\3\1\32\6\0\4\1\1\5"
    u"\1\1\24\0\1\27\1\10\1\30\3\0\1\22\1\13\2\1\1\21"
    u"\1\14\5\0\1\23\1\0\1\15\3\0\1\16\1\24\1\17\1\20"
    u"\5\0\1\25\1\0\1\26\uff82\0");

// uint16_t is now defined
const std::vector<uint16_t> Yylex::ZZ_CMAP = Yylex::zzUnpackCMap(Yylex::ZZ_CMAP_PACKED);

const std::u16string Yylex::ZZ_ACTION_PACKED_0 = u16_lit(u"\2\0\2\1\1\2\1\3\1\4\3\1\1\5\1\6"
    u"\1\7\1\10\1\11\1\12\1\13\1\14\1\15\5\0"
    u"\1\14\1\16\1\17\1\20\1\21\1\22\1\23\1\24"
    u"\1\0\1\25\1\0\1\25\4\0\1\26\1\27\2\0"
    u"\1\30");

const std::vector<int> Yylex::ZZ_ACTION = Yylex::zzUnpackAction(Yylex::ZZ_ACTION_PACKED_0);

const std::u16string Yylex::ZZ_ROWMAP_PACKED_0 = u16_lit(u"\0\0\0\33\0\66\0\121\0\154\0\207\0\66\0\242"
    u"\0\275\0\330\0\66\0\66\0\66\0\66\0\66\0\66"
    u"\0\363\0\u010e\0\66\0\u0129\0\u0144\0\u015f\0\u017a\0\u0195"
    u"\0\66\0\66\0\66\0\66\0\66\0\66\0\66\0\66"
    u"\0\u01b0\0\u01cb\0\u01e6\0\u01e6\0\u0201\0\u021c\0\u0237\0\u0252"
    u"\0\66\0\66\0\u026d\0\u0288\0\66");

const std::vector<int> Yylex::ZZ_ROWMAP = Yylex::zzUnpackRowMap(Yylex::ZZ_ROWMAP_PACKED_0);

const int Yylex::ZZ_TRANS[] = {
     2,  2,  3,  4,  2,  2,  2,  5,  2,  6,  2,  2,  7,  8,  2,  9,
     2,  2,  2,  2,  2, 10, 11, 12, 13, 14, 15, 16, 16, 16, 16, 16,
    16, 16, 16, 17, 18, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1,  4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  4,
    19, 20, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 20, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  5, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 22, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, 23, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, 16, 16, 16, 16, 16, 16, 16, 16, -1, -1, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, -1,
    -1, -1, -1, -1, -1, -1, -1, 24, 25, 26, 27, 28, 29, 30, 31, 32,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 33, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, 34, 35, -1, -1, 34, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 36, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 37, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    39, -1, 39, -1, 39, -1, -1, -1, -1, -1, 39, 39, -1, -1, -1, -1,
    39, 39, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 33, -1, 20,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 20, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, 35, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 40, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, 41, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    42, -1, 42, -1, 42, -1, -1, -1, -1, -1, 42, 42, -1, -1, -1, -1,
    42, 42, -1, -1, -1, -1, -1, -1, -1, -1, -1, 43, -1, 43, -1, 43,
    -1, -1, -1, -1, -1, 43, 43, -1, -1, -1, -1, 43, 43, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, 44, -1, 44, -1, 44, -1, -1, -1, -1, -1,
    44, 44, -1, -1, -1, -1, 44, 44, -1, -1, -1, -1, -1, -1, -1, -1,
    -1,
};


const char* const Yylex::ZZ_ERROR_MSG[3] = {
    "Unkown internal scanner error",
    "Error: could not match input",
    "Error: pushback value was too large"
};

const std::u16string Yylex::ZZ_ATTRIBUTE_PACKED_0 = u16_lit(u"\2\0\1\11\3\1\1\11\3\1\6\11\2\1\1\11"
    u"\5\0\10\11\1\0\1\1\1\0\1\1\4\0\2\11"
    u"\2\0\1\11");

const std::vector<int> Yylex::ZZ_ATTRIBUTE = Yylex::zzUnpackAttribute(Yylex::ZZ_ATTRIBUTE_PACKED_0);

// Constructors

Yylex::Yylex(std::istream& in)
    : m_owned(nullptr), zzReader(&in), zzBuffer(ZZ_BUFFERSIZE, 0), sb() {
}

Yylex::Yylex(std::unique_ptr<std::istream> in_own)
    : m_owned(std::move(in_own)), zzReader(m_owned.get()), zzBuffer(ZZ_BUFFERSIZE, 0), sb() {
}

// Public API

int Yylex::getPosition() const {
    return yychar;
}

void Yylex::yyclose() {
    zzAtEOF = true;
    zzEndRead = zzStartRead;
    if (m_owned) {
        m_owned.reset(); // destroy stream to "close" if it's an fstream
        zzReader = nullptr;
    }
}

void Yylex::yyreset(std::istream& reader) {
    m_owned.reset();
    zzReader = &reader;
    zzAtBOL = true;
    zzAtEOF = false;
    zzEndRead = 0;
    zzStartRead = 0;
    zzCurrentPos = 0;
    zzMarkedPos = 0;
    yyline = 0;
    yychar = 0;
    yycolumn = 0;
    zzLexicalState = YYINITIAL;
}

int Yylex::yystate() const {
    return zzLexicalState;
}

void Yylex::yybegin(int newState) {
    zzLexicalState = newState;
}

std::string Yylex::yytext() const {
    int len = zzMarkedPos - zzStartRead;
    if (len <= 0) return std::string();
    return std::string(&zzBuffer[zzStartRead], static_cast<size_t>(len));
}

char Yylex::yycharat(int pos) const {
    return zzBuffer[zzStartRead + pos];
}

int Yylex::yylength() const {
    return zzMarkedPos - zzStartRead;
}

void Yylex::zzScanError(int errorCode) {
    const char* message = nullptr;
    if (errorCode >= 0 && errorCode < 3) {
        message = ZZ_ERROR_MSG[errorCode];
    } else {
        message = ZZ_ERROR_MSG[ZZ_UNKNOWN_ERROR];
    }
    throw std::runtime_error(message); // Consider using ParseException if it derives from std::exception
}

void Yylex::yypushback(int number) {
    if (number > yylength()) {
        zzScanError(ZZ_PUSHBACK_2BIG);
    }
    zzMarkedPos -= number;
}

// Internals

// zzUnpackCMap now correctly declared due to <cstdint> in header
std::vector<uint16_t> Yylex::zzUnpackCMap(const std::u16string& packed) {
    std::vector<uint16_t> map(0x10000, 0);
    size_t i = 0;
    size_t j = 0;
    while (i + 1 < packed.size()) {
        int count = static_cast<int>(packed[i++]);
        uint16_t value = static_cast<uint16_t>(packed[i++]);
        do {
            if (j < map.size()) map[j] = value;
            ++j;
        } while (--count > 0);
    }
    return map;
}


std::vector<int> Yylex::zzUnpackAction(const std::u16string& packed) {
    std::vector<int> result(45, 0);
    size_t i = 0;
    size_t j = 0;
    while (i + 1 < packed.size() && j < result.size()) {
        int count = static_cast<int>(packed[i++]);
        int value = static_cast<int>(packed[i++]);
        do {
            result[j++] = value;
        } while (--count > 0 && j < result.size());
    }
    return result;
}

std::vector<int> Yylex::zzUnpackRowMap(const std::u16string& packed) {
    std::vector<int> result(45, 0);
    size_t i = 0;
    size_t j = 0;
    while (i + 1 < packed.size() && j < result.size()) {
        int high = static_cast<int>(packed[i++]) << 16;
        result[j++] = high | static_cast<int>(packed[i++]);
    }
    return result;
}

std::vector<int> Yylex::zzUnpackAttribute(const std::u16string& packed) {
    std::vector<int> result(45, 0);
    size_t i = 0;
    size_t j = 0;
    while (i + 1 < packed.size() && j < result.size()) {
        int count = static_cast<int>(packed[i++]);
        int value = static_cast<int>(packed[i++]);
        do {
            result[j++] = value;
        } while (--count > 0 && j < result.size());
    }
    return result;
}

bool Yylex::zzRefill() {
    if (!zzReader) return true;

    // Make room
    if (zzStartRead > 0) {
        int move = zzEndRead - zzStartRead;
        if (move > 0) {
            std::memmove(&zzBuffer[0], &zzBuffer[zzStartRead], static_cast<size_t>(move));
        }
        zzEndRead -= zzStartRead;
        zzCurrentPos -= zzStartRead;
        zzMarkedPos -= zzStartRead;
        zzStartRead = 0;
    }

    // Ensure buffer capacity
    if (zzCurrentPos >= static_cast<int>(zzBuffer.size())) {
        size_t newSize = static_cast<size_t>(zzCurrentPos) * 2;
        if (newSize == 0) newSize = ZZ_BUFFERSIZE;
        zzBuffer.resize(newSize);
    }

    // Read new input
    if (!zzReader->good()) {
        if (zzReader->eof()) return true;
        // Handle other stream errors if necessary
    }

    const std::streamsize space = static_cast<std::streamsize>(zzBuffer.size() - static_cast<size_t>(zzEndRead));
    if (space > 0) {
        zzReader->read(&zzBuffer[zzEndRead], space);
        std::streamsize numRead = zzReader->gcount();
        if (numRead > 0) {
            zzEndRead += static_cast<int>(numRead);
            return false;
        }
         // Handle case where read returns 0 but not EOF (e.g., non-blocking stream)
         // For standard streams, this usually indicates EOF or error.
         // Let's rely on the get() check below.
    }

    // Check EOF explicitly after trying to read
    int c = zzReader->get();
    if (c == EOF) {
         return true;
    } else {
        // BUG FIX: We read a character. Add it to the buffer and return false (not EOF).
        // This mirrors the Java logic (zzBuffer[zzEndRead++] = (char) c)
        // and prevents an infinite loop where unget() would cause this
        // function to be called again with no change in state.
        
        // Ensure buffer has space (should be guaranteed by logic at start of function)
        if (static_cast<size_t>(zzEndRead) < zzBuffer.size()) {
             zzBuffer[static_cast<size_t>(zzEndRead++)] = static_cast<char>(c);
        } else {
             // Buffer is full AND we read one more char. Resize is needed.
             // This logic is slightly different from Java but necessary for C++ vector.
             zzBuffer.resize(zzBuffer.size() * 2);
             zzBuffer[static_cast<size_t>(zzEndRead++)] = static_cast<char>(c);
        }
        
        return false;
    }

    // If space was 0, it means buffer is full, technically not EOF yet
    // but we can't read more without resizing, which zzRefill handles.
    // However, the logic above should handle resizing first.
    // If we reach here, it implies an issue or zzBuffer size is too small.
    // Assuming standard streams, returning true (EOF) might be safer if read fails.
    // return true; // Or throw an error if buffer full is unexpected
}


// yylex implementation

std::unique_ptr<Yytoken> Yylex::yylex() {
    int zzInput;
    int zzAction;

    int zzCurrentPosL;
    int zzMarkedPosL;
    int zzEndReadL = zzEndRead;
    std::vector<char>& zzBufferL = zzBuffer;
    // ZZ_CMAP should now be std::vector<uint16_t>
    const std::vector<uint16_t>& zzCMapL = ZZ_CMAP;

    const int* zzTransL = ZZ_TRANS;
    const std::vector<int>& zzRowMapL = ZZ_ROWMAP;
    const std::vector<int>& zzAttrL = ZZ_ATTRIBUTE;

    while (true) {
        zzMarkedPosL = zzMarkedPos;

        yychar += zzMarkedPosL - zzStartRead;

        zzAction = -1;

        zzCurrentPosL = zzCurrentPos = zzStartRead = zzMarkedPosL;

        zzState = ZZ_LEXSTATE[zzLexicalState];

        // Action loop
        {
            while (true) {
                if (zzCurrentPosL < zzEndReadL) {
                    zzInput = static_cast<unsigned char>(zzBufferL[static_cast<size_t>(zzCurrentPosL++)]);
                } else if (zzAtEOF) {
                    zzInput = YYEOF;
                    break;
                } else {
                    zzCurrentPos = zzCurrentPosL;
                    zzMarkedPos = zzMarkedPosL;
                    bool eof = zzRefill();
                    zzCurrentPosL = zzCurrentPos;
                    zzMarkedPosL = zzMarkedPos;
                    zzEndReadL = zzEndRead;
                    if (eof) {
                        zzInput = YYEOF;
                        break;
                    } else {
                        zzInput = static_cast<unsigned char>(zzBufferL[static_cast<size_t>(zzCurrentPosL++)]);
                    }
                }

                int cmapIndex = (zzInput == YYEOF) ? 0 : static_cast<int>(zzCMapL[static_cast<size_t>(zzInput)]);
                int row = zzRowMapL[static_cast<size_t>(zzState)];
                int zzNext = zzTransL[row + cmapIndex];
                if (zzNext == -1) break;
                zzState = zzNext;

                int zzAttributes = zzAttrL[static_cast<size_t>(zzState)];
                if ((zzAttributes & 1) == 1) {
                    zzAction = zzState;
                    zzMarkedPosL = zzCurrentPosL;
                    if ((zzAttributes & 8) == 8) break;
                }
            }
        }

        zzMarkedPos = zzMarkedPosL;

        int actionIndex = (zzAction < 0) ? zzAction : ZZ_ACTION[static_cast<size_t>(zzAction)];
        // FIX 3: Replace std::any() with Yytoken::Value{} or Yytoken::Value{value}
        switch (actionIndex) {
            case 11: { // APPEND_SB
                sb.append(yytext());
                break;
            }
            case 4: { // STRING_BEGIN_STATE
                sb.clear();
                yybegin(STRING_BEGIN);
                break;
            }
            case 16: { // PUSH_BACKSLASH_B
                sb.push_back('\b');
                break;
            }
            case 6: { // TYPE_RIGHT_BRACE
                return std::make_unique<Yytoken>(Yytoken::TYPE_RIGHT_BRACE, Yytoken::Value{});
            }
            case 23: { // TYPE_VALUE_BOOL
                const std::string t = yytext();
                bool val = (t == "true");
                return std::make_unique<Yytoken>(Yytoken::TYPE_VALUE, Yytoken::Value{val});
            }
            case 22: { // TYPE_VALUE_NULL
                return std::make_unique<Yytoken>(Yytoken::TYPE_VALUE, Yytoken::Value{nullptr}); // Use nullptr for null
            }
            case 13: { // TYPE_VALUE_STRING
                yybegin(YYINITIAL);
                return std::make_unique<Yytoken>(Yytoken::TYPE_VALUE, Yytoken::Value{sb}); // Use string value
            }
            case 12: { // PUSH_BACKSLASH
                sb.push_back('\\');
                break;
            }
            case 21: { // TYPE_VALUE_DOUBLE
                const std::string t = yytext();
                try {
                    double val = std::stod(t);
                    return std::make_unique<Yytoken>(Yytoken::TYPE_VALUE, Yytoken::Value{val});
                } catch (const std::exception& e) {
                    // ParseException definition is now included
                    throw ParseException(yychar, ParseException::ERROR_UNEXPECTED_EXCEPTION, std::string("Invalid double: ") + t + " (" + e.what() + ")");
                }
            }
            case 1: { // ERROR
                // ParseException definition is now included
                throw ParseException(yychar, ParseException::ERROR_UNEXPECTED_CHAR, std::any(yycharat(0))); // ParseException likely takes std::any
            }
            case 8: { // TYPE_RIGHT_SQUARE
                return std::make_unique<Yytoken>(Yytoken::TYPE_RIGHT_SQUARE, Yytoken::Value{});
            }
            case 19: { // PUSH_BACKSLASH_R
                sb.push_back('\r');
                break;
            }
            case 15: { // PUSH_BACKSLASH_SOLIDUS
                sb.push_back('/');
                break;
            }
            case 10: { // TYPE_COLON
                return std::make_unique<Yytoken>(Yytoken::TYPE_COLON, Yytoken::Value{});
            }
            case 14: { // PUSH_BACKSLASH_QUOTE
                sb.push_back('"');
                break;
            }
            case 5: { // TYPE_LEFT_BRACE
                return std::make_unique<Yytoken>(Yytoken::TYPE_LEFT_BRACE, Yytoken::Value{});
            }
            case 17: { // PUSH_BACKSLASH_F
                sb.push_back('\f');
                break;
            }
            case 24: { // PUSH_UNICODE
                try {
                    std::string t = yytext();
                    if (t.size() >= 6 && t.substr(0, 2) == "\\u") {
                        std::string hex = t.substr(2);
                        unsigned int ch_val = std::stoul(hex, nullptr, 16);
                        // Convert Unicode code point to UTF-8 and append to sb
                        if (ch_val <= 0x7F) {
                            sb.push_back(static_cast<char>(ch_val));
                        } else if (ch_val <= 0x7FF) {
                            sb.push_back(static_cast<char>(0xC0 | (ch_val >> 6)));
                            sb.push_back(static_cast<char>(0x80 | (ch_val & 0x3F)));
                        } else if (ch_val <= 0xFFFF) {
                            sb.push_back(static_cast<char>(0xE0 | (ch_val >> 12)));
                            sb.push_back(static_cast<char>(0x80 | ((ch_val >> 6) & 0x3F)));
                            sb.push_back(static_cast<char>(0x80 | (ch_val & 0x3F)));
                        } else if (ch_val <= 0x10FFFF) {
                            sb.push_back(static_cast<char>(0xF0 | (ch_val >> 18)));
                            sb.push_back(static_cast<char>(0x80 | ((ch_val >> 12) & 0x3F)));
                            sb.push_back(static_cast<char>(0x80 | ((ch_val >> 6) & 0x3F)));
                            sb.push_back(static_cast<char>(0x80 | (ch_val & 0x3F)));
                        } else {
                             throw std::runtime_error("Invalid Unicode code point");
                        }
                    } else {
                        throw std::runtime_error("Invalid unicode escape format");
                    }
                } catch (const std::exception& e) {
                    // ParseException definition is now included
                    throw ParseException(yychar, ParseException::ERROR_UNEXPECTED_EXCEPTION, std::string("Unicode escape error: ") + e.what());
                }
                break;
            }
            case 20: { // PUSH_BACKSLASH_T
                sb.push_back('\t');
                break;
            }
            case 7: { // TYPE_LEFT_SQUARE
                return std::make_unique<Yytoken>(Yytoken::TYPE_LEFT_SQUARE, Yytoken::Value{});
            }
            case 2: { // TYPE_VALUE_LONG
                const std::string t = yytext();
                try {
                    long long val = std::stoll(t);
                    return std::make_unique<Yytoken>(Yytoken::TYPE_VALUE, Yytoken::Value{val});
                } catch (const std::exception& e) {
                    // ParseException definition is now included
                    throw ParseException(yychar, ParseException::ERROR_UNEXPECTED_EXCEPTION, std::string("Invalid long: ") + t + " (" + e.what() + ")");
                }
            }
            case 18: { // PUSH_BACKSLASH_N
                sb.push_back('\n');
                break;
            }
            case 9: { // TYPE_COMMA
                return std::make_unique<Yytoken>(Yytoken::TYPE_COMMA, Yytoken::Value{});
            }
            case 3: { // SKIP
                // Skip (e.g., whitespace) - no token needed
                break;
            }
            default: {
                // Check for EOF first - this is the normal end-of-input case
                if (zzInput == YYEOF) {
                    zzAtEOF = true;
                    return nullptr; // End of input
                }
                // Also handle the case where we're at the same position
                if (zzStartRead == zzCurrentPos) {
                    zzAtEOF = true;
                    return nullptr;
                }
                
                // Only now do we have an actual error - unexpected input
                std::cerr << "LEXER ERROR: Unmatched input at position " << zzCurrentPos << std::endl;
                std::cerr << "Current char code: " << static_cast<int>(zzInput) << " (" << static_cast<char>(zzInput) << ")" << std::endl;
                std::cerr << "Buffer context: [";
                for (int i = std::max(0, zzStartRead - 20); i < std::min((int)zzBuffer.size(), zzCurrentPos + 20); i++) {
                    std::cerr << static_cast<char>(zzBuffer[i]);
                }
                std::cerr << "]" << std::endl;
                
                zzScanError(ZZ_NO_MATCH);
            }
        }
    }
}


} // namespace parser
} // namespace json
} // namespace utils
} // namespace minima
} // namespace org
