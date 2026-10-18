#include "org/minima/kissvm/functions/sha/p_r_o_o_f.hpp"

#include <exception>
#include <utility>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/kissvm/values/string_value.hpp"

#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"

// Needed so the compiler knows MiniData/MiniString inherit Streamable
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/utils/streamable.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace sha {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::values::BooleanValue;
using org::minima::kissvm::values::HexValue;
using org::minima::kissvm::values::NumberValue;
using org::minima::kissvm::values::StringValue;
using org::minima::kissvm::values::Value;

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniString;
using org::minima::objects::mmr::MMRData;
using org::minima::objects::mmr::MMRProof;

PROOF::PROOF()
    : MinimaFunction("PROOF") {
}

std::unique_ptr<Value> PROOF::runFunction(Contract& zContract) {
    // Check exact parameter count
    checkExactParamNumber(requiredParams());

    //
    // FIX 1: Change 'NumberValue' to 'auto' to capture the std::unique_ptr
    //
    auto sumval = zContract.getNumberParam(1, *this);

    // Build initial MMR leaf data from param 0 (HEX or SCRIPT)
    std::unique_ptr<MMRData> mmrdata;

    // Try HEX first
    bool gotHex = false;
    try {
        //
        // FIX 2: Change 'HexValue' to 'auto'
        //
        auto hex = zContract.getHexParam(0, *this);
        //
        // FIX 3: Use '->' operator on smart pointers
        //
        const MiniData& md = hex->getMiniData();
        const MiniNumber& sum = sumval->getNumber();
        // Create leaf from MiniData
        mmrdata = MMRData::CreateMMRDataLeafNode(
            const_cast<MiniData&>(md),
            sum
        );
        gotHex = true;
    } catch (const ExecutionException&) {
        // Not HEX, will try SCRIPT
    }

    if (!gotHex) {
        //
        // FIX 4: Change 'StringValue' to 'auto'
        //
        auto scr = zContract.getStringParam(0, *this);
        //
        // FIX 5: Use '->' operator on smart pointers
        //
        const MiniString& ms = scr->getMiniString();
        const MiniNumber& sum = sumval->getNumber();
        mmrdata = MMRData::CreateMMRDataLeafNode(
            const_cast<MiniString&>(ms),
            sum
        );
    }

    //
    // FIX 6: Change 'HexValue' and 'NumberValue' to 'auto'
    //
    auto roothex = zContract.getHexParam(2, *this);
    auto rootsum = zContract.getNumberParam(3, *this);
    //
    // FIX 7: Use '->' operator on smart pointers
    //
    MMRData mmrroot(roothex->getMiniData(), rootsum->getNumber());

    //
    // FIX 8: Change 'HexValue' to 'auto'
    //
    auto chain = zContract.getHexParam(4, *this);

    // Create the MMRProof from the MiniData
    MMRProof proof;
    try {
        //
        // FIX 9: Use '->' operator on smart pointer
        //
        proof = MMRProof::convertMiniDataVersion(chain->getMiniData());
    } catch (const std::exception&) {
        //
        // FIX 10: Use '->' operator on smart pointer
        //
        throw ExecutionException(std::string("Invalid MMRProof at PROOF ") + chain->toString());
    }

    // Calculate the final chain value
    std::unique_ptr<MMRData> root = proof.calculateProof(*mmrdata);

    // Compare with provided root
    bool same = false;
    if (root) {
        same = root->isEqual(mmrroot);
    }

    // Return boolean result
    return std::make_unique<BooleanValue>(same);
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> PROOF::getNewFunction() {
    return std::make_unique<PROOF>();
}

int PROOF::requiredParams() {
    return 5;
}

} // namespace sha
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org
