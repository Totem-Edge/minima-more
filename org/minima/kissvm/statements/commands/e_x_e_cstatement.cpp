#include "org/minima/kissvm/statements/commands/e_x_e_cstatement.hpp"

#include <vector>
#include <stdexcept>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"
#include "org/minima/kissvm/values/string_value.hpp"
#include "org/minima/kissvm/statements/statement_block.hpp"
#include "org/minima/kissvm/statements/statement_parser.hpp"
#include "org/minima/kissvm/tokens/script_tokenizer.hpp"
#include "org/minima/kissvm/tokens/script_token.hpp"

#ifdef _WIN32
// No Windows-specific logic required here.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace statements {
namespace commands {

EXECstatement::EXECstatement(std::unique_ptr<org::minima::kissvm::expressions::Expression> zScript)
    : mScript(std::move(zScript)) {}

EXECstatement::~EXECstatement() = default;
EXECstatement::EXECstatement(EXECstatement&&) noexcept = default;
EXECstatement& EXECstatement::operator=(EXECstatement&&) noexcept = default;

void EXECstatement::execute(org::minima::kissvm::Contract& zContract) {
    using org::minima::kissvm::exceptions::ExecutionException;
    using org::minima::kissvm::values::StringValue;
    using org::minima::kissvm::tokens::ScriptTokenizer;
    using org::minima::kissvm::tokens::ScriptToken;
    using org::minima::kissvm::statements::StatementBlock;
    using org::minima::kissvm::statements::StatementParser;

    // Get the script value from the expression and ensure it's a StringValue
    StringValue* scriptVal = nullptr;
    try {
        auto* val = mScript->getValue(zContract);
        scriptVal = dynamic_cast<StringValue*>(val);
        if (!scriptVal) {
            throw ExecutionException("EXEC requires a STRING expression");
        }
    } catch (const ExecutionException&) {
        throw; // Propagate as-is
    } catch (const std::exception& e) {
        throw ExecutionException(e.what());
    } catch (...) {
        throw ExecutionException("Unknown error evaluating EXEC expression");
    }

    try {
        // Tokenize the script
        ScriptTokenizer tokz(scriptVal->toString());
        // tokz.tokenize() now returns std::vector<ScriptToken>
        std::vector<ScriptToken> utokens = tokz.tokenize();

        // Parse tokens into a StatementBlock
        std::unique_ptr<StatementBlock> block = StatementParser::parseTokens(utokens, zContract.getStackDepth());

        // Execute the parsed block
        block->run(zContract);

    } catch (const ExecutionException& exc) {
        throw; // Match Java: rethrow ExecutionException untouched
    } catch (const std::exception& exc) {
        throw ExecutionException(exc.what());
    } catch (...) {
        throw ExecutionException("Unknown exception during EXEC script execution");
    }
}

std::string EXECstatement::toString() const {
    // Expression interface does not expose a toString; provide a generic representation.
    return "EXEC [EXPR]";
}

} // namespace commands
} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org
