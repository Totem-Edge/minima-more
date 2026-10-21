#include "org/minima/kissvm/contract.hpp"

#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>

#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/exceptions/minima_parse_exception.hpp"
#include "org/minima/kissvm/functions/minima_function.hpp"
#include "org/minima/kissvm/statements/statement_block.hpp"
#include "org/minima/kissvm/statements/statement_parser.hpp"
#include "org/minima/kissvm/tokens/script_token.hpp"
#include "org/minima/kissvm/tokens/script_tokenizer.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/string_value.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"

#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/state_variable.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

// Expression API (needed by getXParam helpers)
#include "org/minima/kissvm/expressions/expression.hpp"

namespace org {
namespace minima {
namespace kissvm {

// Static accessor for MAX_BITSHIFT
const org::minima::objects::base::MiniNumber& Contract::getMAX_BITSHIFT() {
    return org::minima::objects::base::MiniNumber::TWOFIVESIX();
}

// Destructor and move constructor
Contract::~Contract() = default;
Contract::Contract(Contract&&) noexcept = default;

// Internal parse/build helper
void Contract::parseAndBuild(const std::string& zRamScript,
                             const std::vector<org::minima::objects::base::MiniData>& zSignatures) {
    mCompleteLog.clear();

    // Clean the RAM script (store as-is, like Java assigns directly)
    mRamScript = zRamScript;

    // Reset state
    mSignatures.clear();
    mVariables.clear();
    mGlobals.clear();

    mBlock.reset();
    mSuccess = false;
    mSuccessSet = false;
    mParseOK = false;
    mException = false;
    mExceptionString.clear();

    mNumInstructions = 0;
    mMonotonic = true;

    // Begin trace
    traceLog(std::string("Contract   : ") + mRamScript);
    traceLog(std::string("Size       : ") + std::to_string(mRamScript.size()));

    // Transaction / Witness strings
    traceLog(std::string("Transaction   : ") + mTransaction.toString());
    traceLog(std::string("Witness       : ") + mWitness.toString());

    // Load signatures
    for (const auto& sig : zSignatures) {
        traceLog(std::string("Signature : ") + sig.to0xString());
        mSignatures.emplace_back(std::make_unique<org::minima::kissvm::values::HexValue>(sig));

        // org::minima::utils::MinimaLogger::log("DEBUG_CONSTRUCT: Added " + std::to_string(mSignatures.size()) + " signatures to mSignatures");
        // for (size_t i = 0; i < mSignatures.size(); ++i) {
        //     if (mSignatures[i]) {
        //         org::minima::utils::MinimaLogger::log("DEBUG_CONSTRUCT: mSignatures[" + std::to_string(i) + "] = " + 
        //             mSignatures[i]->getMiniData().to0xString());
        //     }
        // }
    }

    // State Variables in Transaction
    {
        auto& svs = mTransaction.getCompleteState();
        for (const auto& svup : svs) {
            if (svup) {
                traceLog("State[" + std::to_string(svup->getPort()) + "] : " + svup->toString());
            }
        }
    }

    // Previous state already set by constructor; trace it here
    if (!mPrevState.empty()) {
        for (const auto& sv : mPrevState) {
            if (sv) {
                traceLog("PrevState[" + std::to_string(sv->getPort()) + "] : " + sv->toString());
            }
        }
    }

    // Parse tokens and build StatementBlock
    try {
        // Tokenize using ScriptToken static API to match StatementParser interface
        std::vector<org::minima::kissvm::tokens::ScriptToken> tokens =
            org::minima::kissvm::tokens::ScriptToken::tokenize(zRamScript);

        // // DEBUG BLOCK
        // int count = 0;
        // for (const auto& tok : tokens) {
        //     traceLog(std::to_string(count++) + ") Token : [" + tok.getTokenTypeString() + "] " + tok.getToken());

        //     std::cerr << count << ") " << tok.getTokenType() << "=" << tok.getTokenTypeString() 
        //       << " [" << tok.getToken() << "]\n" << std::flush;
        // }
        // // DEBUG END

        // Convert tokens to StatementBlock
        resetStackDepth();
        mBlock = org::minima::kissvm::statements::StatementParser::parseTokens(tokens, 0);

        traceLog("Script token parse OK.");
        mParseOK = true;
    } catch (const std::exception& e) {
        mException = true;
        mExceptionString = e.what();
        
        // FOR DEBUG:
        // std::cerr << "!!! PARSE EXCEPTION CAUGHT !!!\n";
        // std::cerr << "Script: [" << zRamScript << "]\n";
        // std::cerr << "Error: " << e.what() << "\n\n";
        // std::cerr << std::flush;
        
        traceLog(std::string("PARSE ERROR : ") + mExceptionString);
    }
}

// Constructors
Contract::Contract(const std::string& zRamScript,
                   const std::string& /*zSignatures*/,
                   org::minima::objects::Witness& zWitness,
                   org::minima::objects::Transaction& zTransaction,
                   std::vector<std::unique_ptr<org::minima::objects::StateVariable>> zPrevState)
    : mTransaction(zTransaction),
      mWitness(zWitness),
      mPrevState(std::move(zPrevState)) {
    
    // int my_id = ++global_contract_counter;
    // std::cerr << "\n╔════════════════════════════════════════╗\n";
    // std::cerr << "║ CONTRACT #" << my_id << " CONSTRUCTOR CALLED      ║\n";
    // std::cerr << "╠════════════════════════════════════════╣\n";
    // std::cerr << "║ Address: " << (void*)this << "        ║\n";
    // std::cerr << "║ Sigs to load: " << zSignatures.size() << "                      ║\n";
    // std::cerr << "╚════════════════════════════════════════╝\n";
    // std::cerr << std::flush;

    mTraceON = false;
    std::vector<org::minima::objects::base::MiniData> sigKeys = zWitness.getAllSignatureKeys();
    
    // std::cerr << "DEBUG_CONTRACT_INIT: Extracted " << sigKeys.size() << " signatures from witness\n";
    // for (const auto& key : sigKeys) {
    //     std::cerr << "  - " << key.to0xString() << "\n";
    // }
    // std::cerr << std::flush;

    parseAndBuild(zRamScript, sigKeys);

    // std::cerr << "CONTRACT #" << my_id << " INITIALIZED\n";
    // std::cerr << "  mSignatures.size() = " << mSignatures.size() << "\n";
    // std::cerr << "  mParseOK = " << mParseOK << "\n";
    // std::cerr << std::flush;
}

Contract::Contract(const std::string& zRamScript,
                   const std::string& /*zSignatures*/,
                   org::minima::objects::Witness& zWitness,
                   org::minima::objects::Transaction& zTransaction,
                   std::vector<std::unique_ptr<org::minima::objects::StateVariable>> zPrevState,
                   bool zTraceON)
    : mTransaction(zTransaction),
      mWitness(zWitness),
      mPrevState(std::move(zPrevState)) {

    // int my_id = ++global_contract_counter;
    // std::cerr << "\n╔════════════════════════════════════════╗\n";
    // std::cerr << "║ CONTRACT #" << my_id << " CONSTRUCTOR CALLED      ║\n";
    // std::cerr << "╠════════════════════════════════════════╣\n";
    // std::cerr << "║ Address: " << (void*)this << "        ║\n";
    // std::cerr << "║ Sigs to load: " << zSignatures.size() << "                      ║\n";
    // std::cerr << "╚════════════════════════════════════════╝\n";
    // std::cerr << std::flush;

    mTraceON = zTraceON;
    std::vector<org::minima::objects::base::MiniData> sigKeys = zWitness.getAllSignatureKeys();
    
    // if (zTraceON) {
    //     std::cerr << "DEBUG_CONTRACT_INIT: Extracted " << sigKeys.size() << " signatures from witness\n";
    //     for (const auto& key : sigKeys) {
    //         std::cerr << "  - " << key.to0xString() << "\n";
    //     }
    //     std::cerr << std::flush;
    // }

    parseAndBuild(zRamScript, sigKeys);

    // std::cerr << "CONTRACT #" << my_id << " INITIALIZED\n";
    // std::cerr << "  mSignatures.size() = " << mSignatures.size() << "\n";
    // std::cerr << "  mParseOK = " << mParseOK << "\n";
    // std::cerr << std::flush;
}

Contract::Contract(const std::string& zRamScript,
                   const std::vector<org::minima::objects::base::MiniData>& zSignatures,
                   org::minima::objects::Witness& zWitness,
                   org::minima::objects::Transaction& zTransaction,
                   std::vector<std::unique_ptr<org::minima::objects::StateVariable>> zPrevState)
    : mTransaction(zTransaction),
      mWitness(zWitness),
      mPrevState(std::move(zPrevState)) {
    
    
    // int my_id = ++global_contract_counter;
    // std::cerr << "\n╔════════════════════════════════════════╗\n";
    // std::cerr << "║ CONTRACT #" << my_id << " CONSTRUCTOR CALLED      ║\n";
    // std::cerr << "╠════════════════════════════════════════╣\n";
    // std::cerr << "║ Address: " << (void*)this << "        ║\n";
    // std::cerr << "║ Sigs to load: " << zSignatures.size() << "                      ║\n";
    // std::cerr << "╚════════════════════════════════════════╝\n";
    // std::cerr << std::flush;
    
    mTraceON = false;
    parseAndBuild(zRamScript, zSignatures);

    // std::cerr << "CONTRACT #" << my_id << " INITIALIZED\n";
    // std::cerr << "  mSignatures.size() = " << mSignatures.size() << "\n";
    // std::cerr << "  mParseOK = " << mParseOK << "\n";
    // std::cerr << std::flush;
}

Contract::Contract(const std::string& zRamScript,
                   const std::vector<org::minima::objects::base::MiniData>& zSignatures,
                   org::minima::objects::Witness& zWitness,
                   org::minima::objects::Transaction& zTransaction,
                   std::vector<std::unique_ptr<org::minima::objects::StateVariable>> zPrevState,
                   bool zTrace)
    : mTransaction(zTransaction),
      mWitness(zWitness),
      mPrevState(std::move(zPrevState)) {
    
    // int my_id = ++global_contract_counter;
    // std::cerr << "\n╔════════════════════════════════════════╗\n";
    // std::cerr << "║ CONTRACT #" << my_id << " CONSTRUCTOR CALLED      ║\n";
    // std::cerr << "╠════════════════════════════════════════╣\n";
    // std::cerr << "║ Address: " << (void*)this << "        ║\n";
    // std::cerr << "║ Sigs to load: " << zSignatures.size() << "                      ║\n";
    // std::cerr << "╚════════════════════════════════════════╝\n";
    // std::cerr << std::flush;
    
    mTraceON = zTrace;
    parseAndBuild(zRamScript, zSignatures);

    // std::cerr << "CONTRACT #" << my_id << " INITIALIZED\n";
    // std::cerr << "  mSignatures.size() = " << mSignatures.size() << "\n";
    // std::cerr << "  mParseOK = " << mParseOK << "\n";
    // std::cerr << std::flush;
}

// Globals environment
void Contract::setGlobals(const org::minima::objects::base::MiniNumber& zBlock,
                          const org::minima::objects::base::MiniNumber& zBlockTimeMilli,
                          org::minima::objects::Transaction& zTrx,
                          int zInput,
                          const org::minima::objects::base::MiniNumber& zInputBlkCreate,
                          const std::string& zScript) {
    // Get the Coin
    auto& inputs = zTrx.getAllInputs();
    if (zInput < 0 || static_cast<std::size_t>(zInput) >= inputs.size() || !inputs[zInput]) {
        throw std::invalid_argument("Invalid input index in setGlobals");
    }
    org::minima::objects::Coin& cc = *inputs[zInput];

    // Set the environment
    setGlobalVariable("@BLOCK", std::make_unique<org::minima::kissvm::values::NumberValue>(zBlock));
    setGlobalVariable("@BLOCKMILLI", std::make_unique<org::minima::kissvm::values::NumberValue>(zBlockTimeMilli));

    setGlobalVariable("@CREATED", std::make_unique<org::minima::kissvm::values::NumberValue>(zInputBlkCreate));
    setGlobalVariable("@COINAGE", std::make_unique<org::minima::kissvm::values::NumberValue>(zBlock.sub(zInputBlkCreate)));

    setGlobalVariable("@INPUT", std::make_unique<org::minima::kissvm::values::NumberValue>(zInput));
    setGlobalVariable("@COINID", std::make_unique<org::minima::kissvm::values::HexValue>(cc.getCoinID()));

    // AMOUNT is the amount of Minima or Token(Scaled)..
    org::minima::objects::base::MiniNumber amt = cc.getAmount();
    if (!cc.getTokenID().isEqual(org::minima::objects::Token::TOKENID_MINIMA)) {
        // Scale the amount
        auto tok = cc.getToken();
        if (tok) {
            auto scaled = tok->getScaledTokenAmount(amt);
            if (scaled) {
                amt = *scaled;
            }
        }
    }
    setGlobalVariable("@AMOUNT", std::make_unique<org::minima::kissvm::values::NumberValue>(amt));

    setGlobalVariable("@ADDRESS", std::make_unique<org::minima::kissvm::values::HexValue>(cc.getAddress()));
    setGlobalVariable("@TOKENID", std::make_unique<org::minima::kissvm::values::HexValue>(cc.getTokenID()));
    setGlobalVariable("@SCRIPT", std::make_unique<org::minima::kissvm::values::StringValue>(zScript));

    setGlobalVariable("@TOTIN", std::make_unique<org::minima::kissvm::values::NumberValue>(
                                    static_cast<int>(zTrx.getAllInputs().size())));
    setGlobalVariable("@TOTOUT", std::make_unique<org::minima::kissvm::values::NumberValue>(
                                     static_cast<int>(zTrx.getAllOutputs().size())));
}

void Contract::setGlobalVariable(const std::string& zGlobal,
                                 std::unique_ptr<org::minima::kissvm::values::Value> zValue) {
    if (!zValue) {
        throw std::invalid_argument("setGlobalVariable null value");
    }
    mGlobals[zGlobal] = std::move(zValue);
    traceLog("Global [" + zGlobal + "] : " + mGlobals[zGlobal]->toString());
}

const org::minima::kissvm::values::Value& Contract::getGlobal(const std::string& zGlobal) const {
    auto it = mGlobals.find(zGlobal);
    if (it == mGlobals.end() || !it->second) {
        throw org::minima::kissvm::exceptions::ExecutionException("Global not found - " + zGlobal);
    }

    // Will this break monotonic (Java checks when read)
    if (zGlobal == "@BLOCK" || zGlobal == "@BLOCKMILLI" || zGlobal == "@COINAGE") {
        // const method; but Java mutates mMonotonic. Emulate via const_cast.
        const_cast<Contract*>(this)->mMonotonic = false;
    }
    return *(it->second);
}

const std::unordered_map<std::string, std::unique_ptr<org::minima::kissvm::values::Value>>&
Contract::getGlobalVariables() const {
    return mGlobals;
}

void Contract::setAllGlobalVariables(
    std::unordered_map<std::string, std::unique_ptr<org::minima::kissvm::values::Value>> zGlobals) {
    mGlobals = std::move(zGlobals);
}

bool Contract::isParseOK() const { return mParseOK; }
bool Contract::isException() const { return mException; }
std::string Contract::getException() const { return mExceptionString; }
bool Contract::isTrace() const { return mTraceON; }

void Contract::traceLog(const std::string& zLog) {
    if (isTrace()) {
        org::minima::utils::MinimaLogger::log("INST[" + std::to_string(mNumInstructions) + "] STACK[" +
                                              std::to_string(getStackDepth()) + "] - " + zLog);
    }
    mCompleteLog += "INST[" + std::to_string(mNumInstructions) + "] - " + zLog + "\n";
}

std::string Contract::getCompleteTraceLog() const { return mCompleteLog; }

void Contract::incrementInstructions() { incrementInstructions(1); }

void Contract::incrementInstructions(int zNum) {
    mNumInstructions += zNum;
    if (mNumInstructions > mMaxInstructions) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            "MAX instruction number reached! " + std::to_string(mNumInstructions));
    }
}

int Contract::getNumberOfInstructions() const { return mNumInstructions; }

void Contract::setMaxInstructions(int zMax) { mMaxInstructions = zMax; }

void Contract::resetStackDepth() { mStackDepth = 0; }
int Contract::getStackDepth() const { return mStackDepth; }

void Contract::incrementStackDepth() {
    ++mStackDepth;
    if (mStackDepth > MAX_STACK_DEPTH) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            "Stack depth too deep! (MAX " + std::to_string(MAX_STACK_DEPTH) + ") " + std::to_string(mStackDepth));
    }
}

void Contract::decrementStackDepth() {
    --mStackDepth;
    if (mStackDepth < 0) {
        mStackDepth = 0;
    }
}

void Contract::run() {

    if (mException) {
        std::cerr << "    *** Exception: " << mExceptionString << " ***\n";
    }
    std::cerr << std::flush;

    if (!mParseOK) {
        std::cerr << "!!! RETURNING EARLY - PARSE FAILED !!!\n";
        if (mException) {
            std::cerr << "!!! Exception was: " << mExceptionString << " !!!\n";
        }
        std::cerr << std::flush;
        traceLog("Script parse FAILED. Please fix and retry.");
        return;
    }

    // Reset Stack Depth
    resetStackDepth();

    // Run
    try {
        traceLog("Start executing the contract");
        if (mBlock) {
            mBlock->run(*this);
        }
    } catch (const std::exception& e) {
        if (mTraceON) {
            org::minima::utils::MinimaLogger::log(e);
        }

        mException = true;
        mExceptionString = e.what();

        // Automatic fail
        traceLog(std::string("Execution Error - ") + e.what());

        mSuccess = false;
        mSuccessSet = true;
    }

    traceLog("Contract instructions : " + std::to_string(mNumInstructions));
    traceLog(std::string("Contract finished     : ") + (mSuccess ? "TRUE" : "FALSE"));
}

void Contract::setRETURNValue(bool zSUCCESS) {
    if (!mSuccessSet) {
        mSuccess = zSUCCESS;
        mSuccessSet = true;
    }
}

bool Contract::isSuccess() const { return mSuccess; }
bool Contract::isSuccessSet() const { return mSuccessSet; }
bool Contract::isMonotonic() const { return mMonotonic; }

std::string Contract::getMiniScript() const { return mRamScript; }
org::minima::objects::Transaction& Contract::getTransaction() const { return mTransaction; }
org::minima::objects::Witness& Contract::getWitness() const { return mWitness; }

org::minima::utils::json::JSONObject Contract::getAllVariables() const {
    org::minima::utils::json::JSONObject variables;

    for (const auto& kv : mVariables) {
        std::string key = kv.first;
        const auto& valptr = kv.second;

        if (key.find(',') != std::string::npos) {
            std::replace(key.begin(), key.end(), ',', ' ');
            key = "( " + key + " )";
        }

        variables.put(key, valptr ? std::any(valptr->toString()) : std::any(std::string("")));
    }

    return variables;
}

void Contract::removeVariable(const std::string& zName) {
    mVariables.erase(zName);
}

bool Contract::existsVariable(const std::string& zName) const {
    return mVariables.find(zName) != mVariables.end();
}

const org::minima::kissvm::values::Value* Contract::getVariable(const std::string& zName) const {
    auto it = mVariables.find(zName);
    if (it == mVariables.end() || !it->second) {
        return nullptr;
    }
    return it->second.get();
}

void Contract::setVariable(const std::string& zName,
                           std::unique_ptr<org::minima::kissvm::values::Value> zValue) {
    if (!zValue) {
        throw std::invalid_argument("setVariable null value");
    }
    mVariables[zName] = std::move(zValue);
    traceVariables();
}

// Typed parameter helpers
std::unique_ptr<org::minima::kissvm::values::NumberValue>
Contract::getNumberParam(int zParamNumber, org::minima::kissvm::functions::MinimaFunction& zFunction) {
    auto& expr = zFunction.getParameter(zParamNumber);
    org::minima::kissvm::values::Value* vv = expr.getValue(*this);
    if (!vv || vv->getValueType() != org::minima::kissvm::values::Value::VALUE_NUMBER) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            "Incorrect Parameter type - should be NumberValue @ " + std::to_string(zParamNumber) + " " +
            zFunction.getName());
    }
    auto* num = dynamic_cast<org::minima::kissvm::values::NumberValue*>(vv);
    if (!num) {
        throw org::minima::kissvm::exceptions::ExecutionException("Parameter dynamic_cast failed (NumberValue)");
    }
    auto ret = std::make_unique<org::minima::kissvm::values::NumberValue>(num->getNumber());
    return ret;
}

std::unique_ptr<org::minima::kissvm::values::HexValue>
Contract::getHexParam(int zParamNumber, org::minima::kissvm::functions::MinimaFunction& zFunction) {
    auto& expr = zFunction.getParameter(zParamNumber);
    org::minima::kissvm::values::Value* vv = expr.getValue(*this);
    if (!vv || vv->getValueType() != org::minima::kissvm::values::Value::VALUE_HEX) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            "Incorrect Parameter type - should be HEXValue @ " + std::to_string(zParamNumber) + " " +
            zFunction.getName());
    }
    auto* hv = dynamic_cast<org::minima::kissvm::values::HexValue*>(vv);
    if (!hv) {
        throw org::minima::kissvm::exceptions::ExecutionException("Parameter dynamic_cast failed (HexValue)");
    }
    auto ret = std::make_unique<org::minima::kissvm::values::HexValue>(hv->getMiniData());
    return ret;
}

std::unique_ptr<org::minima::kissvm::values::StringValue>
Contract::getStringParam(int zParamNumber, org::minima::kissvm::functions::MinimaFunction& zFunction) {
    auto& expr = zFunction.getParameter(zParamNumber);
    org::minima::kissvm::values::Value* vv = expr.getValue(*this);
    if (!vv || vv->getValueType() != org::minima::kissvm::values::Value::VALUE_SCRIPT) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            "Incorrect Parameter type - should be ScriptValue @ " + std::to_string(zParamNumber) + " " +
            zFunction.getName());
    }
    auto* sv = dynamic_cast<org::minima::kissvm::values::StringValue*>(vv);
    if (!sv) {
        throw org::minima::kissvm::exceptions::ExecutionException("Parameter dynamic_cast failed (StringValue)");
    }
    auto ret = std::make_unique<org::minima::kissvm::values::StringValue>(sv->toString());
    return ret;
}

std::unique_ptr<org::minima::kissvm::values::BooleanValue>
Contract::getBoolParam(int zParamNumber, org::minima::kissvm::functions::MinimaFunction& zFunction) {
    auto& expr = zFunction.getParameter(zParamNumber);
    org::minima::kissvm::values::Value* vv = expr.getValue(*this);
    if (!vv || vv->getValueType() != org::minima::kissvm::values::Value::VALUE_BOOLEAN) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            "Incorrect Parameter type - should be BooleanValue @ " + std::to_string(zParamNumber) + " " +
            zFunction.getName());
    }
    auto* bv = dynamic_cast<org::minima::kissvm::values::BooleanValue*>(vv);
    if (!bv) {
        throw org::minima::kissvm::exceptions::ExecutionException("Parameter dynamic_cast failed (BooleanValue)");
    }
    auto ret = std::make_unique<org::minima::kissvm::values::BooleanValue>(bv->isTrue());
    return ret;
}

// State access
std::unique_ptr<org::minima::kissvm::values::Value> Contract::getState(int zStateNum) const {
    if (!mTransaction.stateExists(zStateNum)) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            "State Variable does not exist " + std::to_string(zStateNum));
    }
    const org::minima::objects::StateVariable* sv = mTransaction.getStateValue(zStateNum);
    if (!sv) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            "State Variable null " + std::to_string(zStateNum));
    }
    std::string stateval = sv->toString();
    return org::minima::kissvm::values::Value::getValue(stateval);
}

std::unique_ptr<org::minima::kissvm::values::Value> Contract::getPrevState(int zPrev) const {
    for (const auto& sv : mPrevState) {
        if (sv && sv->getPort() == zPrev) {
            std::string stateval = sv->toString();
            return org::minima::kissvm::values::Value::getValue(stateval);
        }
    }
    throw org::minima::kissvm::exceptions::ExecutionException(
        "PREVSTATE Missing : " + std::to_string(zPrev));
}

// Variables tracing
void Contract::traceVariables() {
    std::string varlist = "{ ";

    for (const auto& kv : mVariables) {
        std::string key = kv.first;
        const auto& valptr = kv.second;

        if (key.find(',') != std::string::npos) {
            std::replace(key.begin(), key.end(), ',', ' ');
            key = "( " + key + " )";
        }
        varlist += key + " = " + (valptr ? valptr->toString() : std::string("")) + ", ";
    }

    traceLog(varlist + "}");
}



// Signature check
// DEBUG VERSION of Contract::checkSignature
// Add this to see what's happening inside the method

bool Contract::checkSignature(const org::minima::kissvm::values::HexValue& zPublicKey) const {

    const org::minima::objects::base::MiniData& checksig = zPublicKey.getMiniData();
    
    // for (size_t i = 0; i < mSignatures.size(); i++) {
    //     if (mSignatures[i]) {
    //         std::cerr << "  mSignatures[" << i << "] = " 
    //                   << mSignatures[i]->getMiniData().to0xString() << "\n";
    //     }
    // }
    
    for (const auto& sig : mSignatures) {
        if (sig && sig->getMiniData().isEqual(checksig)) {
            return true;
        }
    }
    
    return false;
}

std::string Contract::cleanScript(const std::string& zScript) {
    return cleanScript(zScript, false);
}

std::string Contract::cleanScript(const std::string& zScript, bool zLog) {
    // Collapse all whitespace first (Java: replaceAll("\\s+"," ").trim()).
    std::string script;
    bool inSpace = false;
    for (unsigned char c : zScript) {
        if (std::isspace(c)) {
            if (!script.empty() && !inSpace) script.push_back(' ');
            inSpace = true;
        } else {
            script.push_back(static_cast<char>(c));
            inSpace = false;
        }
    }
    while (!script.empty() && script.back() == ' ') script.pop_back();
    while (!script.empty() && script.front() == ' ') script.erase(script.begin());

    std::string ret;
    try {
        using org::minima::kissvm::tokens::ScriptToken;
        using org::minima::kissvm::tokens::ScriptTokenizer;

        ScriptTokenizer tokz(script, true);
        auto tokens = tokz.tokenize();

        const std::vector<std::string> bools = {
            "LT","LTE","GT","GTE","EQ","NEQ","XOR","AND","OR","NXOR","NAND","NOR","NOT","NEG"};
        const std::vector<std::string> eows = {
            "+","-","/","*","%","&","|","^","~","=","(",")","[","]","<",">","<<",">>"};
        const std::vector<std::string> afters = {
            "+","-","/","*","%","&","|","^","=","<",">","<<",">>",")"};

        auto ends = [](const std::string& value, char c) {
            return !value.empty() && value.back() == c;
        };
        auto contains = [](const std::vector<std::string>& list, const std::string& value) {
            return std::find(list.begin(), list.end(), value) != list.end();
        };

        bool whites = true;
        std::string prev;
        for (const auto& tok : tokens) {
            if (zLog) {
                org::minima::utils::MinimaLogger::log(tok.toString());
            }
            const std::string t = tok.getToken();
            const int type = tok.getTokenType();

            if (type == ScriptToken::TOKEN_COMMAND) {
                if (!ret.empty() && ret.back() == ' ') ret += t + " ";
                else ret += " " + t + " ";
                whites = true;
            } else if (contains(bools, t)) {
                ret += " " + t + " ";
                whites = true;
            } else if (t.rfind("0x", 0) == 0 || t.rfind("0X", 0) == 0) {
                std::string hex = "0x" + t.substr(2);
                std::transform(hex.begin() + 2, hex.end(), hex.begin() + 2,
                               [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
                if (ends(prev, ')')) {
                    ret += " " + hex;
                } else if (whites) {
                    ret += hex;
                } else {
                    ret += " " + hex;
                }
                whites = false;
            } else if (!t.empty() && (t[0] == '(' || t[0] == '[')) {
                if (whites) {
                    const bool space = ends(prev, ')') || ends(prev, ']');
                    if (space) ret += " " + t;
                    else ret += t;
                } else {
                    const bool space = contains(eows, prev);
                    if (space) ret += " " + t;
                    else ret += t;
                }
                whites = true;
            } else {
                const bool afterClose = ends(prev, ')') || ends(prev, ']');
                if (afterClose && !contains(afters, t)) {
                    ret += " " + t;
                    whites = false;
                } else if (ScriptTokenizer::isWhiteSpace(t) || contains(eows, t)) {
                    ret += t;
                    whites = true;
                } else {
                    if (whites) ret += t;
                    else ret += " " + t;
                    whites = false;
                }
            }
            prev = t;
        }
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(std::string("Clean Script Error @ ") + zScript + " " + e.what());
        return zScript;
    }

    while (!ret.empty() && ret.back() == ' ') ret.pop_back();
    while (!ret.empty() && ret.front() == ' ') ret.erase(ret.begin());
    return ret;
}

} // namespace kissvm
} // namespace minima
} // namespace org