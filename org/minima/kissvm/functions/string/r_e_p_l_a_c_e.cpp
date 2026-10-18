#include "org/minima/kissvm/functions/string/r_e_p_l_a_c_e.hpp"

#include <regex>
#include <memory>
#include <stdexcept>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/string_value.hpp"
#include "org/minima/kissvm/values/value.hpp"

#ifdef _WIN32
// No OS-specific behavior needed for this file currently.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace string {

namespace {
// Escape a string for use as a literal in an ECMAScript std::regex
// We escape characters with special meaning in ECMAScript regex.
std::string escapeRegexECMAScript(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (char c : s) {
        switch (c) {
            case '\\': case '^': case '$': case '.':
            case '|': case '?': case '*': case '+':
            case '(': case ')': case '{': case '}':
            case '[': case ']':
                out.push_back('\\');
                out.push_back(c);
                break;
            default:
                out.push_back(c);
                break;
        }
    }
    return out;
}
} // anonymous namespace

REPLACE::REPLACE() : MinimaFunction("REPLACE") {}

std::unique_ptr<org::minima::kissvm::values::Value>
REPLACE::runFunction(org::minima::kissvm::Contract& zContract) {
    // Throws ExecutionException on mismatch
    checkExactParamNumber(requiredParams());

    // Get parameters as StringValue (owned)
    std::unique_ptr<org::minima::kissvm::values::StringValue> strmain = zContract.getStringParam(0, *this);
    std::unique_ptr<org::minima::kissvm::values::StringValue> strsearch = zContract.getStringParam(1, *this);
    std::unique_ptr<org::minima::kissvm::values::StringValue> strrepl = zContract.getStringParam(2, *this);

    const std::string main  = strmain ? strmain->toString() : std::string();
    const std::string search = strsearch ? strsearch->toString() : std::string();
    const std::string repl   = strrepl ? strrepl->toString() : std::string();

    // Perform safe replacement with length checks
    const std::string newstr = safeReplaceAll(main, search, repl);

    return std::make_unique<org::minima::kissvm::values::StringValue>(newstr);
}

int REPLACE::requiredParams() {
    return 3;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> REPLACE::getNewFunction() {
    return std::make_unique<REPLACE>();
}

std::string REPLACE::safeReplaceAll(const std::string& zStart,
                                    const std::string& zSearch,
                                    const std::string& zReplace) {
    using org::minima::kissvm::Contract;
    using org::minima::kissvm::exceptions::ExecutionException;

    // Special cases to keep behavior reasonable
    if (zStart.empty()) {
        return std::string();
    }

    // Create a literal regex pattern like Java Pattern.quote(zSearch)
    const std::string quoted = escapeRegexECMAScript(zSearch);
    const std::regex pattern(quoted, std::regex::ECMAScript);

    std::string result;
    result.reserve(zStart.size());

    std::sregex_iterator it(zStart.begin(), zStart.end(), pattern);
    std::sregex_iterator end;

    std::size_t last_pos = 0;

    for (; it != end; ++it) {
        const std::smatch& m = *it;

        // Append the literal segment before the match
        const std::size_t match_pos = static_cast<std::size_t>(m.position());
        if (match_pos > last_pos) {
            result.append(zStart, last_pos, match_pos - last_pos);
            if (result.size() > static_cast<std::size_t>(Contract::MAX_DATA_SIZE)) {
                throw ExecutionException("Replace String too long! " + std::to_string(result.size()));
            }
        }

        // Append the formatted replacement (ECMAScript semantics, similar to Java appendReplacement)
        const std::string formatted = m.format(zReplace);
        result.append(formatted);
        if (result.size() > static_cast<std::size_t>(Contract::MAX_DATA_SIZE)) {
            throw ExecutionException("Replace String too long! " + std::to_string(result.size()));
        }

        last_pos = match_pos + static_cast<std::size_t>(m.length());
    }

    // Append the tail
    if (last_pos < zStart.size()) {
        result.append(zStart, last_pos, std::string::npos);
    }

    if (result.size() > static_cast<std::size_t>(Contract::MAX_DATA_SIZE)) {
        throw ExecutionException("Replace String too long! " + std::to_string(result.size()));
    }

    return result;
}

} // namespace string
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org