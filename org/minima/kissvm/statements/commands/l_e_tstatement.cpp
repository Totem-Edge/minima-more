#include "org/minima/kissvm/statements/commands/l_e_tstatement.hpp"

#include <sstream>
#include <utility>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/utils/minima_logger.hpp"

#ifdef _WIN32
// No platform-specific functionality required here currently.
#endif

namespace {

// Simple trim helper (both ends), mimicking Java's String.trim()
inline std::string trim_copy(const std::string& s) {
    const char* ws = " \t\n\r\f\v";
    const auto start = s.find_first_not_of(ws);
    if (start == std::string::npos) {
        return std::string();
    }
    const auto end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

} // anonymous namespace

namespace org {
namespace minima {
namespace kissvm {
namespace statements {
namespace commands {

// Constructors
LETstatement::LETstatement(
    const std::string& zVariableName,
    std::unique_ptr<org::minima::kissvm::expressions::Expression> zExpression)
    : mLETType(LET_VARIABLE),
      mName(zVariableName),
      mValue(std::move(zExpression)) {}

LETstatement::LETstatement(
    std::vector<std::unique_ptr<org::minima::kissvm::expressions::Expression>> zArrayPos,
    std::unique_ptr<org::minima::kissvm::expressions::Expression> zExpression)
    : mLETType(LET_ARRAY),
      mArrayPos(std::move(zArrayPos)),
      mValue(std::move(zExpression)) {}

// Special members for unique_ptr to forward-declared types (PITFALL 1)
LETstatement::~LETstatement() = default;
LETstatement::LETstatement(LETstatement&&) noexcept = default;
LETstatement& LETstatement::operator=(LETstatement&&) noexcept = default;

void LETstatement::execute(org::minima::kissvm::Contract& zContract) {
    using org::minima::kissvm::expressions::Expression;
    using org::minima::kissvm::exceptions::ExecutionException;
    using org::minima::kissvm::values::Value;

    if (mLETType == LET_VARIABLE) {
        // Evaluate the value expression and set the variable
        std::unique_ptr<Value> val_ptr(mValue ? mValue->getValue(zContract) : nullptr);
        zContract.setVariable(mName, std::move(val_ptr));
    } else {
        // Build the composite array position key (CSV of indices with trailing comma)
        std::string pos;

        for (const auto& exp_up : mArrayPos) {
            // Evaluate each index expression
            std::unique_ptr<Value> vv(exp_up ? exp_up->getValue(zContract) : nullptr);

            // MUST be a number
            if (!vv || vv->getValueType() != Value::VALUE_NUMBER) {
                const std::string vvs = vv ? vv->toString() : std::string("null");
                throw ExecutionException(
                    "Incorrect Parameter type for LET - MUST be a NumberValue @ " + trim_copy(vvs));
            }

            // Append trimmed string plus comma
            pos += trim_copy(vv->toString());
            pos += ",";
        }

        // Now ask the contract to set that array variable..
        std::unique_ptr<Value> val_ptr(mValue ? mValue->getValue(zContract) : nullptr);
        zContract.setVariable(pos, std::move(val_ptr));
    }
}

std::string LETstatement::toString() const {
    using org::minima::utils::MinimaLogger;

    if (mLETType == LET_VARIABLE) {
        // We cannot safely call Expression::toString (not in the interface),
        // so we use a placeholder for the expression.
        return std::string("LET ") + mName + " = [expr]";
    }

    // It's an array LET
    std::string let = "LET ( ";

    try {
        // Append a placeholder per index expression to mirror count/spacing
        for (size_t i = 0; i < mArrayPos.size(); ++i) {
            let += "[expr] ";
        }

        // Trim trailing whitespace and finish formatting
        let = trim_copy(let) + " ) = [expr]";
    } catch (const std::exception& serious) {
        MinimaLogger::log(std::string("**SERIOUS ERROR in LET (caught) ") + serious.what());
    } catch (...) {
        MinimaLogger::log(std::string("**SERIOUS ERROR in LET (caught) unknown exception"));
    }

    return let;
}

} // namespace commands
} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org