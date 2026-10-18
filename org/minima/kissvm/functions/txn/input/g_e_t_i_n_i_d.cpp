#include "org/minima/kissvm/functions/txn/input/g_e_t_i_n_i_d.hpp"

#include <string>
#include <vector>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"

#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/witness.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace txn {
namespace input {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::values::Value;
using org::minima::kissvm::values::HexValue;

GETINID::GETINID() : MinimaFunction("GETINID") {}

std::unique_ptr<Value> GETINID::runFunction(Contract& zContract) {
    // Ensure exact number of parameters
    checkExactParamNumber(requiredParams());

    // Which input index
    int input = zContract.getNumberParam(0, *this)->getNumber().getAsInt();

    // Get the Transaction
    org::minima::objects::Transaction& trans = zContract.getTransaction();

    // Check input exists
    const auto& ins = trans.getAllInputs();
    if (input < 0 || static_cast<size_t>(input) >= ins.size()) {
        throw ExecutionException(
            "Input number out of range " + std::to_string(input) + "/" + std::to_string(ins.size()));
    }

    // Use the witness data: fetch the Coin via the corresponding CoinProof
    org::minima::objects::Witness& wit = zContract.getWitness();
    const auto& proofs = wit.getAllCoinProofs();
    const auto& coin_from_proof = proofs.at(static_cast<size_t>(input))->getCoin();

    // Return the coin ID as HexValue
    return std::make_unique<HexValue>(coin_from_proof.getCoinID());
}

int GETINID::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> GETINID::getNewFunction() {
    return std::make_unique<GETINID>();
}

} // namespace input
} // namespace txn
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org