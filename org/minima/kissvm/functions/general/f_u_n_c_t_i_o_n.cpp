#include "org/minima/kissvm/functions/general/f_u_n_c_t_i_o_n.hpp"

#include <stdexcept>
#include <vector>
#include <string>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/functions/string/r_e_p_l_a_c_e.hpp"
#include "org/minima/kissvm/statements/statement_block.hpp"
#include "org/minima/kissvm/statements/statement_parser.hpp"
#include "org/minima/kissvm/tokens/script_token.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"
#include "org/minima/kissvm/values/string_value.hpp"
#include "org/minima/kissvm/values/value.hpp"

// Expression header to call Expression::getValue
#include "org/minima/kissvm/expressions/expression.hpp"

#ifdef _WIN32
// No OS-specific logic required for this translation
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {

const std::string FUNCTION::FUNCTION_RETURN = "returnvalue";

FUNCTION::FUNCTION() : MinimaFunction("FUNCTION") {}

namespace {
// ... (anonymous namespace with tokenizerCountTokensMinusOne remains unchanged) ...
// Replicates Java's new StringTokenizer(str, "$").countTokens() - 1 behavior.
// It counts the number of non-empty segments of non-'$' characters separated by one or more '$',
// then subtracts 1. Leading/trailing '$' and consecutive '$' are ignored as delimiters (no empty tokens).
int tokenizerCountTokensMinusOne(const std::string& s) {
    int segments = 0;
    bool in_segment = false;
    for (char c : s) {
        if (c == '$') {
            if (in_segment) {
                in_segment = false;
            }
        } else {
            if (!in_segment) {
                in_segment = true;
                ++segments;
            }
        }
    }
    return (segments > 0) ? (segments - 1) : -1;
}
} // anonymous namespace

std::unique_ptr<org::minima::kissvm::values::Value>
FUNCTION::runFunction(org::minima::kissvm::Contract& zContract) {
    using org::minima::kissvm::exceptions::ExecutionException;
    using org::minima::kissvm::values::Value;
    using org::minima::kissvm::values::StringValue;
    using org::minima::kissvm::values::BooleanValue;
    using org::minima::kissvm::tokens::ScriptToken;
    using org::minima::kissvm::statements::StatementBlock;
    using org::minima::kissvm::statements::StatementParser;

    checkMinParamNumber(requiredParams());

    //
    // FIX 1: 'script' must be a unique_ptr to accept the return type
    //
    std::unique_ptr<StringValue> script = zContract.getStringParam(0, *this);
    //
    // FIX 1 (consequence): Use '->' operator on the pointer
    //
    std::string finalfunction = script->toString();

    // Check number of replacements via tokenizer semantics
    if (tokenizerCountTokensMinusOne(finalfunction) > 64) {
        throw ExecutionException("Too many replacements in FUNCTION, max 64");
    }

    // Replace all the $ variables
    int params = static_cast<int>(getAllParameters().size());
    for (int i = 1; i < params; ++i) {
        // Evaluate the parameter
        auto paramval = getParameter(i).getValue(zContract);

        // Build search token
        std::string search = std::string("$") + std::to_string(i);

        // What type is it
        if (paramval->getValueType() == Value::VALUE_SCRIPT) {
            //
            // FIX 2: Add full namespace to 'REPLACE'
            //
            finalfunction = org::minima::kissvm::functions::string::REPLACE::safeReplaceAll(finalfunction, search, "[" + paramval->toString() + "]");
        } else {
            //
            // FIX 2: Add full namespace to 'REPLACE'
            //
            finalfunction = org::minima::kissvm::functions::string::REPLACE::safeReplaceAll(finalfunction, search, paramval->toString());
        }

        // Check number of replacements again
        if (tokenizerCountTokensMinusOne(finalfunction) > 64) {
            throw ExecutionException("Too many replacements in FUNCTION, max 64");
        }
    }

    // Remove any previous return vars
    zContract.removeVariable(FUNCTION_RETURN);

    try {
        // Tokenize the script (use ScriptToken::tokenize to get std::vector<ScriptToken>)
        std::vector<ScriptToken> tokens = ScriptToken::tokenize(finalfunction);

        // Convert to a statement block
        std::unique_ptr<StatementBlock> mBlock = StatementParser::parseTokens(tokens, zContract.getStackDepth());

        // Run it
        mBlock->run(zContract);
    } catch (const ExecutionException& exc) {
        throw;
    } catch (const std::exception& exc) {
        throw ExecutionException(std::string(exc.what()));
    }

    // Is there a return variable
    if (zContract.existsVariable(FUNCTION_RETURN)) {
        //
        // FIX 3: Change 'Value*' to 'const Value*' to match return type
        //
        const Value* ret = zContract.getVariable(FUNCTION_RETURN);
        if (!ret) {
            // Defensive: Shouldn't happen if existsVariable returned true
            return std::make_unique<BooleanValue>(true);
        }
        // Recreate a new Value from its string representation
        return Value::getValue(ret->toString());
    }

    return std::make_unique<BooleanValue>(true);
}

bool FUNCTION::isRequiredMinimumParameterNumber() const {
    return true;
}

int FUNCTION::requiredParams() {
    return 1;
}

std::unique_ptr<MinimaFunction> FUNCTION::getNewFunction() {
    return std::make_unique<FUNCTION>();
}

} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org
