#include "org/minima/kissvm/functions/minima_function.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>

#include "org/minima/kissvm/expressions/expression.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/exceptions/minima_parse_exception.hpp"

// Include all function headers to build the prototype registry
#include "org/minima/kissvm/functions/hex/c_o_n_c_a_t.hpp"
#include "org/minima/kissvm/functions/hex/l_e_n.hpp"
#include "org/minima/kissvm/functions/hex/r_e_v.hpp"
#include "org/minima/kissvm/functions/hex/s_u_b_s_e_t.hpp"
#include "org/minima/kissvm/functions/general/g_e_t.hpp"
#include "org/minima/kissvm/functions/general/e_x_i_s_t_s.hpp"
#include "org/minima/kissvm/functions/general/a_d_d_r_e_s_s.hpp"
#include "org/minima/kissvm/functions/cast/b_o_o_l.hpp"
#include "org/minima/kissvm/functions/cast/h_e_x.hpp"
#include "org/minima/kissvm/functions/cast/n_u_m_b_e_r.hpp"
#include "org/minima/kissvm/functions/cast/s_t_r_i_n_g.hpp"
#include "org/minima/kissvm/functions/cast/a_s_c_i_i.hpp"
#include "org/minima/kissvm/functions/cast/u_t_f8.hpp"
#include "org/minima/kissvm/functions/number/a_b_s.hpp"
#include "org/minima/kissvm/functions/number/c_e_i_l.hpp"
#include "org/minima/kissvm/functions/number/f_l_o_o_r.hpp"
#include "org/minima/kissvm/functions/number/m_a_x.hpp"
#include "org/minima/kissvm/functions/number/m_i_n.hpp"
#include "org/minima/kissvm/functions/number/d_e_c.hpp"
#include "org/minima/kissvm/functions/number/i_n_c.hpp"
#include "org/minima/kissvm/functions/number/s_i_g_d_i_g.hpp"
#include "org/minima/kissvm/functions/number/p_o_w.hpp"
#include "org/minima/kissvm/functions/number/s_q_r_t.hpp"
#include "org/minima/kissvm/functions/general/f_u_n_c_t_i_o_n.hpp"
#include "org/minima/kissvm/functions/txn/input/s_u_m_i_n_p_u_t_s.hpp"
#include "org/minima/kissvm/functions/txn/output/s_u_m_o_u_t_p_u_t_s.hpp"
#include "org/minima/kissvm/functions/hex/s_e_t_l_e_n.hpp"
#include "org/minima/kissvm/functions/string/r_e_p_l_a_c_e.hpp"
#include "org/minima/kissvm/functions/string/r_e_p_l_a_c_e_f_i_r_s_t.hpp"
#include "org/minima/kissvm/functions/string/s_u_b_s_t_r.hpp"
#include "org/minima/kissvm/functions/hex/o_v_e_r_w_r_i_t_e.hpp"
#include "org/minima/kissvm/functions/sha/s_h_a2.hpp"
#include "org/minima/kissvm/functions/sha/s_h_a3.hpp"
#include "org/minima/kissvm/functions/sha/p_r_o_o_f.hpp"
#include "org/minima/kissvm/functions/hex/b_i_t_s_e_t.hpp"
#include "org/minima/kissvm/functions/hex/b_i_t_g_e_t.hpp"
#include "org/minima/kissvm/functions/hex/b_i_t_c_o_u_n_t.hpp"
#include "org/minima/kissvm/functions/sigs/s_i_g_n_e_d_b_y.hpp"
#include "org/minima/kissvm/functions/sigs/m_u_l_t_i_s_i_g.hpp"
#include "org/minima/kissvm/functions/sigs/c_h_e_c_k_s_i_g.hpp"
#include "org/minima/kissvm/functions/txn/input/g_e_t_i_n_a_d_d_r.hpp"
#include "org/minima/kissvm/functions/txn/input/g_e_t_i_n_a_m_t.hpp"
#include "org/minima/kissvm/functions/txn/input/g_e_t_i_n_i_d.hpp"
#include "org/minima/kissvm/functions/txn/input/g_e_t_i_n_t_o_k.hpp"
#include "org/minima/kissvm/functions/txn/input/v_e_r_i_f_y_i_n.hpp"
#include "org/minima/kissvm/functions/txn/output/g_e_t_o_u_t_a_d_d_r.hpp"
#include "org/minima/kissvm/functions/txn/output/g_e_t_o_u_t_a_m_t.hpp"
#include "org/minima/kissvm/functions/txn/output/g_e_t_o_u_t_t_o_k.hpp"
#include "org/minima/kissvm/functions/txn/output/g_e_t_o_u_t_k_e_e_p_s_t_a_t_e.hpp"
#include "org/minima/kissvm/functions/txn/output/v_e_r_i_f_y_o_u_t.hpp"
#include "org/minima/kissvm/functions/state/s_t_a_t_e.hpp"
#include "org/minima/kissvm/functions/state/p_r_e_v_s_t_a_t_e.hpp"
#include "org/minima/kissvm/functions/state/s_a_m_e_s_t_a_t_e.hpp"

#ifdef _WIN32
// No OS-specific behavior required here; placeholder for potential future differences
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {

// Helper: uppercase a string (ASCII)
static std::string to_upper_ascii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

// Build and return the static prototype list once
static std::vector<std::unique_ptr<MinimaFunction>>& prototypeList() {
    static std::vector<std::unique_ptr<MinimaFunction>> s_list;
    if (s_list.empty()) {
        using namespace org::minima::kissvm::functions;

        // Order preserved as in Java ALL_FUNCTIONS
        s_list.emplace_back(std::make_unique<hex::CONCAT>());
        s_list.emplace_back(std::make_unique<hex::LEN>());
        s_list.emplace_back(std::make_unique<hex::REV>());
        s_list.emplace_back(std::make_unique<hex::SUBSET>());
        s_list.emplace_back(std::make_unique<general::GET>());
        s_list.emplace_back(std::make_unique<general::EXISTS>());
        s_list.emplace_back(std::make_unique<general::ADDRESS>());
        s_list.emplace_back(std::make_unique<cast::BOOL>());
        s_list.emplace_back(std::make_unique<cast::HEX>());
        s_list.emplace_back(std::make_unique<cast::NUMBER>());
        s_list.emplace_back(std::make_unique<cast::STRING>());
        s_list.emplace_back(std::make_unique<cast::ASCII>());
        s_list.emplace_back(std::make_unique<cast::UTF8>());
        s_list.emplace_back(std::make_unique<number::ABS>());
        s_list.emplace_back(std::make_unique<number::CEIL>());
        s_list.emplace_back(std::make_unique<number::FLOOR>());
        s_list.emplace_back(std::make_unique<number::MAX>());
        s_list.emplace_back(std::make_unique<number::MIN>());
        s_list.emplace_back(std::make_unique<number::DEC>());
        s_list.emplace_back(std::make_unique<number::INC>());
        s_list.emplace_back(std::make_unique<number::SIGDIG>());
        s_list.emplace_back(std::make_unique<number::POW>());
        s_list.emplace_back(std::make_unique<number::SQRT>());
        s_list.emplace_back(std::make_unique<FUNCTION>());
        s_list.emplace_back(std::make_unique<txn::input::SUMINPUTS>());
        s_list.emplace_back(std::make_unique<txn::output::SUMOUTPUTS>());
        s_list.emplace_back(std::make_unique<SETLEN>());
        s_list.emplace_back(std::make_unique<functions::string::REPLACE>());
        s_list.emplace_back(std::make_unique<functions::string::REPLACEFIRST>());
        s_list.emplace_back(std::make_unique<SUBSTR>());
        s_list.emplace_back(std::make_unique<hex::OVERWRITE>());
        s_list.emplace_back(std::make_unique<sha::SHA2>());
        s_list.emplace_back(std::make_unique<SHA3>());
        s_list.emplace_back(std::make_unique<sha::PROOF>());
        s_list.emplace_back(std::make_unique<hex::BITSET>());
        s_list.emplace_back(std::make_unique<hex::BITGET>());
        s_list.emplace_back(std::make_unique<hex::BITCOUNT>());
        s_list.emplace_back(std::make_unique<sigs::SIGNEDBY>());
        s_list.emplace_back(std::make_unique<MULTISIG>());
        s_list.emplace_back(std::make_unique<sigs::CHECKSIG>());
        s_list.emplace_back(std::make_unique<txn::input::GETINADDR>());
        s_list.emplace_back(std::make_unique<txn::input::GETINAMT>());
        s_list.emplace_back(std::make_unique<txn::input::GETINID>());
        s_list.emplace_back(std::make_unique<txn::input::GETINTOK>());
        s_list.emplace_back(std::make_unique<txn::input::VERIFYIN>());
        s_list.emplace_back(std::make_unique<GETOUTADDR>());
        s_list.emplace_back(std::make_unique<txn::output::GETOUTAMT>());
        s_list.emplace_back(std::make_unique<txn::output::GETOUTTOK>());
        s_list.emplace_back(std::make_unique<GETOUTKEEPSTATE>());
        s_list.emplace_back(std::make_unique<txn::output::VERIFYOUT>());
        s_list.emplace_back(std::make_unique<state::STATE>());
        s_list.emplace_back(std::make_unique<state::PREVSTATE>());
        s_list.emplace_back(std::make_unique<state::SAMESTATE>());
    }
    return s_list;
}

// MinimaFunction implementations

MinimaFunction::MinimaFunction(const std::string& zName)
    : mName(to_upper_ascii(zName)), mParameters() {}

MinimaFunction::~MinimaFunction() = default;
MinimaFunction::MinimaFunction(MinimaFunction&&) noexcept = default;
MinimaFunction& MinimaFunction::operator=(MinimaFunction&&) noexcept = default;

void MinimaFunction::addParameter(std::unique_ptr<org::minima::kissvm::expressions::Expression> zParam) {
    mParameters.emplace_back(std::move(zParam));
}

void MinimaFunction::addParameter(org::minima::kissvm::expressions::Expression* zParam) {
    mParameters.emplace_back(std::unique_ptr<org::minima::kissvm::expressions::Expression>(zParam));
}

org::minima::kissvm::expressions::Expression& MinimaFunction::getParameter(int zParamNum) {
    if (zParamNum >= static_cast<int>(getParameterNum())) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            "Parameter missing for " + getName() + " num:" + std::to_string(zParamNum));
    }
    return *(mParameters[zParamNum]);
}

int MinimaFunction::getParameterNum() const {
    return static_cast<int>(mParameters.size());
}

const std::vector<std::unique_ptr<org::minima::kissvm::expressions::Expression>>&
MinimaFunction::getAllParameters() const {
    return mParameters;
}

const std::string& MinimaFunction::getName() const {
    return mName;
}

void MinimaFunction::checkIsOfType(const org::minima::kissvm::values::Value& zValue, int zType) {
    if ((zValue.getValueType() & zType) == 0) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            std::string("Parameter is incorrect type in ") + getName() +
            " Found:" + org::minima::kissvm::values::Value::getValueTypeString(zValue.getValueType()) +
            " @ " + zValue.toString());
    }
}

void MinimaFunction::checkExactParamNumber(int zNumberOfParams) {
    if (static_cast<int>(getAllParameters().size()) != zNumberOfParams) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            "Function requires " + std::to_string(zNumberOfParams) + " parameters");
    }
}

void MinimaFunction::checkMinParamNumber(int zMinNumberOfParams) {
    if (static_cast<int>(getAllParameters().size()) < zMinNumberOfParams) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            "Function requires minimum of " + std::to_string(zMinNumberOfParams) + " parameters");
    }
}

bool MinimaFunction::isRequiredMinimumParameterNumber() const {
    return false;
}

void MinimaFunction::checkParamNumberCorrect() {
    int paramsize = static_cast<int>(getAllParameters().size());
    int reqparam = requiredParams();

    if (isRequiredMinimumParameterNumber()) {
        if (paramsize < reqparam) {
            // Note: preserves Java's exact spacing: "a  minimum"
            throw org::minima::kissvm::exceptions::MinimaParseException(
                getName() + " function requires a  minimum of " + std::to_string(reqparam) +
                " parameters not " + std::to_string(paramsize));
        }
    } else {
        if (paramsize != reqparam) {
            throw org::minima::kissvm::exceptions::MinimaParseException(
                getName() + " function requires exactly " + std::to_string(reqparam) +
                " parameters not " + std::to_string(paramsize));
        }
    }
}

std::unique_ptr<MinimaFunction> MinimaFunction::getFunction(const std::string& zFunction) {

    std::string lookup = to_upper_ascii(zFunction);
    auto& list = prototypeList();
    for (const auto& func : list) {
        if (func->getName() == lookup) {
            return func->getNewFunction();
        }
    }
    throw org::minima::kissvm::exceptions::MinimaParseException(
        "Invalid Function : " + zFunction);
}

} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org