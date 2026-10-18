#include "org/minima/kissvm/functions/general/g_e_t.hpp"

#include <cctype>
#include <string>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"
#include "org/minima/kissvm/values/value.hpp"

#ifdef _WIN32
// No OS-specific code required for this translation yet.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace general {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::expressions::Expression;
using org::minima::kissvm::values::Value;

namespace {
    // Simple trim helper to mimic Java's String.trim()
    std::string trim_copy(const std::string& s) {
        size_t start = 0;
        while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
            ++start;
        }
        if (start == s.size()) {
            return std::string();
        }
        size_t end = s.size();
        do {
            --end;
        } while (end > start && std::isspace(static_cast<unsigned char>(s[end])));
        return s.substr(start, end - start + 1);
    }
}

GET::GET() : MinimaFunction("GET") {}

std::unique_ptr<Value> GET::runFunction(Contract& zContract) {
    // Ensure minimum number of parameters
    checkMinParamNumber(requiredParams());

    // Build the parameter string with trailing comma
    std::string ps;

    const auto& params = getAllParameters();
    for (const auto& exp : params) {
        // Evaluate expression to a Value (manage memory)
        std::unique_ptr<Value> numval(exp->getValue(zContract));
        // Must be a number
        checkIsOfType(*numval, Value::VALUE_NUMBER);

        // Append trimmed string representation and a comma
        ps += trim_copy(numval->toString());
        ps += ",";
    }

    // Get the Value from the contract
    const Value* val = zContract.getVariable(ps);

    // Must exist
    if (val == nullptr) {
        throw ExecutionException(std::string("GET Variable not found : ") + ps);
    }

    // Return a new Value cloned from the found one via its string representation
    return Value::getValue(val->toString());
}

bool GET::isRequiredMinimumParameterNumber() const {
    return true;
}

int GET::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> GET::getNewFunction() {
    return std::make_unique<GET>();
}

} // namespace general
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org