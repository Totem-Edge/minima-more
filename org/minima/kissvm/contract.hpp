#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace org { namespace minima { namespace kissvm { namespace statements { class StatementBlock; } } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; class NumberValue; class HexValue; class StringValue; class BooleanValue; } } } }
namespace org { namespace minima { namespace kissvm { namespace functions { class MinimaFunction; } } } }
namespace org { namespace minima { namespace kissvm { namespace tokens { class ScriptToken; class ScriptTokenizer; } } } }
namespace org { namespace minima { namespace kissvm { namespace exceptions { class ExecutionException; } } } }

namespace org { namespace minima { namespace objects { class Transaction; class Witness; class StateVariable; class Coin; class Token; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; class MiniNumber; } } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace kissvm {

class Contract {
public:
    // Constants mirroring Java
    static constexpr int MAX_STACK_DEPTH = 64;
    static constexpr int MAX_FUNCTION_PARAMS = 32;
    static constexpr int MAX_DATA_SIZE = 64 * 1024;

    // Access Java's MAX_BITSHIFT via accessor to avoid static init issues
    static const org::minima::objects::base::MiniNumber& getMAX_BITSHIFT();

    // Destructor and move operations due to unique_ptr members (Pitfall 1)
    virtual ~Contract();
    Contract(Contract&&) noexcept;
    Contract& operator=(Contract&&) noexcept = delete; // references prevent move assignment

    // Delete copy
    Contract(const Contract&) = delete;
    Contract& operator=(const Contract&) = delete;

    // Constructors mapping Java overloads
    Contract(const std::string& zRamScript,
             const std::string& zSignatures,
             org::minima::objects::Witness& zWitness,
             org::minima::objects::Transaction& zTransaction,
             std::vector<std::unique_ptr<org::minima::objects::StateVariable>> zPrevState);

    Contract(const std::string& zRamScript,
             const std::string& zSignatures,
             org::minima::objects::Witness& zWitness,
             org::minima::objects::Transaction& zTransaction,
             std::vector<std::unique_ptr<org::minima::objects::StateVariable>> zPrevState,
             bool zTraceON);

    Contract(const std::string& zRamScript,
             const std::vector<org::minima::objects::base::MiniData>& zSignatures,
             org::minima::objects::Witness& zWitness,
             org::minima::objects::Transaction& zTransaction,
             std::vector<std::unique_ptr<org::minima::objects::StateVariable>> zPrevState);

    Contract(const std::string& zRamScript,
             const std::vector<org::minima::objects::base::MiniData>& zSignatures,
             org::minima::objects::Witness& zWitness,
             org::minima::objects::Transaction& zTransaction,
             std::vector<std::unique_ptr<org::minima::objects::StateVariable>> zPrevState,
             bool zTrace);

    // Globals environment
    void setGlobals(const org::minima::objects::base::MiniNumber& zBlock,
                    const org::minima::objects::base::MiniNumber& zBlockTimeMilli,
                    org::minima::objects::Transaction& zTrx,
                    int zInput,
                    const org::minima::objects::base::MiniNumber& zInputBlkCreate,
                    const std::string& zScript);

    void setGlobalVariable(const std::string& zGlobal,
                           std::unique_ptr<org::minima::kissvm::values::Value> zValue);

    const org::minima::kissvm::values::Value& getGlobal(const std::string& zGlobal) const;

    // Accessors for global variables map (read-only); setAll moves ownership
    const std::unordered_map<std::string, std::unique_ptr<org::minima::kissvm::values::Value>>&
    getGlobalVariables() const;

    void setAllGlobalVariables(
        std::unordered_map<std::string, std::unique_ptr<org::minima::kissvm::values::Value>> zGlobals);

    // Parse/exception/trace flags
    bool isParseOK() const;
    bool isException() const;
    std::string getException() const;
    bool isTrace() const;

    // Tracing
    void traceLog(const std::string& zLog);
    std::string getCompleteTraceLog() const;

    // Instructions and stack management
    void incrementInstructions();
    void incrementInstructions(int zNum);
    int getNumberOfInstructions() const;
    void setMaxInstructions(int zMax);

    void resetStackDepth();
    int getStackDepth() const;
    void incrementStackDepth();
    void decrementStackDepth();

    // Execute contract
    void run();

    // RETURN value
    void setRETURNValue(bool zSUCCESS);
    bool isSuccess() const;
    bool isSuccessSet() const;

    // Monotonic
    bool isMonotonic() const;

    // Basic getters
    std::string getMiniScript() const;
    org::minima::objects::Transaction& getTransaction() const;
    org::minima::objects::Witness& getWitness() const;

    // Variables to JSON
    org::minima::utils::json::JSONObject getAllVariables() const;

    // Variables map operations
    void removeVariable(const std::string& zName);
    bool existsVariable(const std::string& zName) const;

    // Returns nullptr if not exists
    const org::minima::kissvm::values::Value* getVariable(const std::string& zName) const;

    void setVariable(const std::string& zName,
                     std::unique_ptr<org::minima::kissvm::values::Value> zValue);

    // Typed parameter helpers (unique_ptr to return clones)
    std::unique_ptr<org::minima::kissvm::values::NumberValue>
    getNumberParam(int zParamNumber, org::minima::kissvm::functions::MinimaFunction& zFunction);

    std::unique_ptr<org::minima::kissvm::values::HexValue>
    getHexParam(int zParamNumber, org::minima::kissvm::functions::MinimaFunction& zFunction);

    std::unique_ptr<org::minima::kissvm::values::StringValue>
    getStringParam(int zParamNumber, org::minima::kissvm::functions::MinimaFunction& zFunction);

    std::unique_ptr<org::minima::kissvm::values::BooleanValue>
    getBoolParam(int zParamNumber, org::minima::kissvm::functions::MinimaFunction& zFunction);

    // State access (returns newly constructed Values)
    std::unique_ptr<org::minima::kissvm::values::Value> getState(int zStateNum) const;
    std::unique_ptr<org::minima::kissvm::values::Value> getPrevState(int zPrev) const;

    // Variable tracing
    void traceVariables();
    
    // Signature check
    bool checkSignature(const org::minima::kissvm::values::HexValue& zPublicKey) const;

    // Cleaning utility
    static std::string cleanScript(const std::string& zScript);
    static std::string cleanScript(const std::string& zScript, bool zLog);

private:
    // Helpers
    void parseAndBuild(const std::string& zRamScript,
                       const std::vector<org::minima::objects::base::MiniData>& zSignatures);

private:
    // Non-owning references to provided objects
    org::minima::objects::Transaction& mTransaction;
    org::minima::objects::Witness&     mWitness;

    // The final version of the script
    std::string mRamScript;

    // Root block
    std::unique_ptr<org::minima::kissvm::statements::StatementBlock> mBlock;

    // Valid signatures
    std::vector<std::unique_ptr<org::minima::kissvm::values::HexValue>> mSignatures;

    // User-defined variables and globals
    std::unordered_map<std::string, std::unique_ptr<org::minima::kissvm::values::Value>> mVariables;
    std::unordered_map<std::string, std::unique_ptr<org::minima::kissvm::values::Value>> mGlobals;

    // Previous state variables (owned copy)
    std::vector<std::unique_ptr<org::minima::objects::StateVariable>> mPrevState;

    // Flags and counters
    bool mMonotonic = true;

    bool mSuccess = false;
    bool mSuccessSet = false;

    bool mTraceON = false;
    bool mParseOK = false;

    bool mException = false;
    std::string mExceptionString;

    int mNumInstructions = 0;
    int mMaxInstructions = 1024;

    int mStackDepth = 0;

    // Log of complete execution
    std::string mCompleteLog;
};

} // namespace kissvm
} // namespace minima
} // namespace org