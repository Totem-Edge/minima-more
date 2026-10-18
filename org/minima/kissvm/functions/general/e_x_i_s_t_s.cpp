#include "org/minima/kissvm/functions/general/e_x_i_s_t_s.hpp"

#include <string>
#include <vector>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"

namespace {

// Simple trim utility to mimic Java String.trim()
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
namespace functions {
namespace general {

EXISTS::EXISTS() : org::minima::kissvm::functions::MinimaFunction("EXISTS") {}

std::unique_ptr<org::minima::kissvm::values::Value>
EXISTS::runFunction(org::minima::kissvm::Contract& zContract) {
    // Check minimum parameter count
    checkMinParamNumber(requiredParams());

    // Build the full parameter string (with trailing commas), using trimmed string values
    std::string ps;

    const auto& params = getAllParameters();
    for (const auto& up_exp : params) {
        org::minima::kissvm::expressions::Expression* exp = up_exp.get();
        org::minima::kissvm::values::Value* numval = exp->getValue(zContract);

        // Ensure it's a number
        checkIsOfType(*numval, org::minima::kissvm::values::Value::VALUE_NUMBER);

        // Append trimmed string value plus comma
        ps += trim_copy(numval->toString());
        ps += ",";
    }

    // Check existence of the variable
    const org::minima::kissvm::values::Value* val = zContract.getVariable(ps);
    if (val == nullptr) {
        return std::make_unique<org::minima::kissvm::values::BooleanValue>(false);
    }

    return std::make_unique<org::minima::kissvm::values::BooleanValue>(true);
}

bool EXISTS::isRequiredMinimumParameterNumber() const {
    return true;
}

int EXISTS::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> EXISTS::getNewFunction() {
    return std::make_unique<EXISTS>();
}

} // namespace general
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org