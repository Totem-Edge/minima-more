#include "org/minima/kissvm/statements/commands/m_a_s_tstatement.hpp"

#include <vector>
#include <utility>
#include <memory>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"
#include "org/minima/kissvm/statements/statement_block.hpp"
#include "org/minima/kissvm/statements/statement_parser.hpp"
#include "org/minima/kissvm/tokens/script_tokenizer.hpp"
#include "org/minima/kissvm/tokens/script_token.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/objects/script_proof.hpp"
#include "org/minima/objects/witness.hpp"
// Include full definitions to use member functions on these types (fix incomplete type usage)
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_string.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace statements {
namespace commands {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::expressions::Expression;
using org::minima::kissvm::statements::StatementBlock;
using org::minima::kissvm::statements::StatementParser;
using org::minima::kissvm::tokens::ScriptToken;
using org::minima::kissvm::tokens::ScriptTokenizer;
using org::minima::kissvm::values::HexValue;
using org::minima::objects::ScriptProof;
using org::minima::objects::Witness;

MASTstatement::MASTstatement(std::unique_ptr<Expression> zMAST)
    : mMASTScript(std::move(zMAST)) {}

MASTstatement::~MASTstatement() = default;
MASTstatement::MASTstatement(MASTstatement&&) noexcept = default;
MASTstatement& MASTstatement::operator=(MASTstatement&&) noexcept = default;

void MASTstatement::execute(Contract& zContract) {
    // Get the MAST value (must be a HexValue)
    Expression* expr = mMASTScript.get();
    if (!expr) {
        throw ExecutionException("MAST requires a valid expression");
    }

    auto* hv = dynamic_cast<HexValue*>(expr->getValue(zContract));
    if (!hv) {
        throw ExecutionException("MAST requires HEX value expression");
    }

    // Get the Witness from the contract
    Witness& wit = zContract.getWitness();

    // Get the Script Proof for the given MAST hash
    ScriptProof* scrpr = wit.getScript(hv->getMiniData());
    if (scrpr == nullptr) {
        throw ExecutionException(std::string("No script found for MAST ") + hv->getMiniData().toString());
    }

    // Get the script of this hash value
    std::string script = scrpr->getScript().toString();

    try {
        // Tokenize the script
        ScriptTokenizer tokz(script);
        // tokz.tokenize() now returns std::vector<ScriptToken>
        std::vector<ScriptToken> utokens = tokz.tokenize();
        

        // Convert the script to a StatementBlock
        std::unique_ptr<StatementBlock> mBlock = StatementParser::parseTokens(utokens, zContract.getStackDepth());

        // Now run it
        mBlock->run(zContract);

    } catch (const ExecutionException&) {
        // Explicitly rethrow ExecutionException
        throw;

    } catch (const std::exception& e) {
        // Wrap other exceptions in an ExecutionException
        throw ExecutionException(e.what());

    } catch (...) {
        throw ExecutionException("Unknown exception during MAST execution");
    }
}

std::string MASTstatement::toString() const {
    // Expression string rendering is not available; provide minimal identifier
    return "MAST";
}

} // namespace commands
} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org
