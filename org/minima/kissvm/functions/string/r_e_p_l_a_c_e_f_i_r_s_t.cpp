#include "org/minima/kissvm/functions/string/r_e_p_l_a_c_e_f_i_r_s_t.hpp"

#include <utility>
#include <string>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/string_value.hpp"
#include "org/minima/kissvm/values/value.hpp"

#ifdef _WIN32
// No OS-specific behavior required
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace string {

REPLACEFIRST::REPLACEFIRST()
    : org::minima::kissvm::functions::MinimaFunction("REPLACEFIRST") {}

std::unique_ptr<org::minima::kissvm::values::Value>
REPLACEFIRST::runFunction(org::minima::kissvm::Contract& zContract) {
    checkExactParamNumber(requiredParams());

    // Get parameters as StringValues
    auto strmain   = zContract.getStringParam(0, *this);
    auto strsearch = zContract.getStringParam(1, *this);
    auto strrepl   = zContract.getStringParam(2, *this);

    const std::string main   = strmain->toString();
    const std::string search = strsearch->toString();
    const std::string repl   = strrepl->toString();

    // If search is empty, Java's replaceFirst with Pattern.quote("") would match empty string at start.
    // Java String.replaceFirst with an empty pattern results in replacement at the beginning once.
    // For literal behavior, emulate: insert repl at the beginning once.
    std::string newstr;
    if (search.empty()) {
        newstr = repl + main;
    } else {
        std::size_t pos = main.find(search);
        if (pos == std::string::npos) {
            newstr = main;
        } else {
            newstr = main.substr(0, pos);
            newstr += repl;
            newstr += main.substr(pos + search.size());
        }
    }

    return std::make_unique<org::minima::kissvm::values::StringValue>(newstr);
}

int REPLACEFIRST::requiredParams() {
    return 3;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction>
REPLACEFIRST::getNewFunction() {
    return std::make_unique<REPLACEFIRST>();
}

} // namespace string
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org