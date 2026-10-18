#pragma once

#include <any>
#include <deque>
#include <istream>
#include <memory>
#include <string>

#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace utils {
namespace json {
namespace parser {

// Forward declarations for collaborators provided elsewhere in the project.
class Yylex;
class Yytoken;
class ParseException;
class ContainerFactory;
class ContentHandler;

class JSONParser {
public:
    // Parser states (mirroring the Java constants)
    static constexpr int S_INIT = 0;
    static constexpr int S_IN_FINISHED_VALUE = 1;
    static constexpr int S_IN_OBJECT = 2;
    static constexpr int S_IN_ARRAY = 3;
    static constexpr int S_PASSED_PAIR_KEY = 4;
    static constexpr int S_IN_PAIR_VALUE = 5;
    static constexpr int S_END = 6;
    static constexpr int S_IN_ERROR = -1;

    JSONParser();
    ~JSONParser();

    // Reset to initial state without resetting the underlying reader.
    void reset();

    // Reset to initial state with a new input stream (reader).
    void reset(std::istream& in);

    // Get the position of the beginning of the current token.
    int getPosition() const;

    // DOM-style parsing into a dynamically-typed value.
    std::any parse(const std::string& s);
    std::any parse(const std::string& s, ContainerFactory* containerFactory);
    std::any parse(std::istream& in);
    std::any parse(std::istream& in, ContainerFactory* containerFactory);

    // Streaming parsing with ContentHandler
    void parse(const std::string& s, ContentHandler& contentHandler);
    void parse(const std::string& s, ContentHandler& contentHandler, bool isResume);
    void parse(std::istream& in, ContentHandler& contentHandler);
    void parse(std::istream& in, ContentHandler& contentHandler, bool isResume);

private:
    // Helper: fetch next token
    void nextToken();

    // Helper: peek status from a stack (front element), or -1 if empty
    static int peekStatus(const std::deque<int>& statusStack);

    // Helpers to create containers (fallback to default if factory is null or returns null)
    std::shared_ptr<org::minima::utils::json::JSONObject> createObjectContainer(ContainerFactory* containerFactory);
    std::shared_ptr<org::minima::utils::json::JSONArray>  createArrayContainer(ContainerFactory* containerFactory);

    // Custom deleter to handle deletion of Yylex while keeping it as an incomplete type in this header.
    struct YylexDeleter {
        void operator()(Yylex* p) const;
    };

private:
    std::unique_ptr<Yylex, YylexDeleter> m_lexer;    // underlying lexer (can be null until reset(in))
    std::unique_ptr<Yytoken> m_token;                // current token
    int m_status;                                     // current parser status

    // For content handler (streaming) mode, to support resume
    std::unique_ptr<std::deque<int>> m_handlerStatusStack;
};

} // namespace parser
} // namespace json
} // namespace utils
} // namespace minima
} // namespace org