#pragma once

#include <memory>
#include <string>
#include <vector>

namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; } } } }
namespace org { namespace minima { namespace kissvm { namespace expressions { class Expression; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace functions {

class MinimaFunction {
public:
    // Construct with a name; stored uppercase like Java
    explicit MinimaFunction(const std::string& zName);

    // Destructor and move operations (Pitfall 1: unique_ptr to incomplete type)
    virtual ~MinimaFunction();
    MinimaFunction(MinimaFunction&&) noexcept;
    MinimaFunction& operator=(MinimaFunction&&) noexcept;

    // Delete copy semantics
    MinimaFunction(const MinimaFunction&) = delete;
    MinimaFunction& operator=(const MinimaFunction&) = delete;

    // Parameter management
    void addParameter(std::unique_ptr<org::minima::kissvm::expressions::Expression> zParam);
    void addParameter(org::minima::kissvm::expressions::Expression* zParam); // convenience, takes ownership

    // Throws ExecutionException if out of bounds
    org::minima::kissvm::expressions::Expression& getParameter(int zParamNum);

    int getParameterNum() const;

    // Access all parameters
    const std::vector<std::unique_ptr<org::minima::kissvm::expressions::Expression>>& getAllParameters() const;

    // Name
    const std::string& getName() const;

protected:
    // Type and parameter checks (throw ExecutionException on failure)
    void checkIsOfType(const org::minima::kissvm::values::Value& zValue, int zType);
    void checkExactParamNumber(int zNumberOfParams);
    void checkMinParamNumber(int zMinNumberOfParams);

public:
    // Execute and return a Value
    virtual std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) = 0;

    // Factory for a new copy of this function
    virtual std::unique_ptr<MinimaFunction> getNewFunction() = 0;

    // How many parameters are required
    virtual int requiredParams() = 0;

    // Can be overridden in classes that set a minimum
    virtual bool isRequiredMinimumParameterNumber() const;

    // External function to do a quick parameter count check (throws MinimaParseException)
    void checkParamNumberCorrect();

    // Static factory lookup by name (case-insensitive); throws MinimaParseException if not found
    static std::unique_ptr<MinimaFunction> getFunction(const std::string& zFunction);

private:
    std::string mName;
    std::vector<std::unique_ptr<org::minima::kissvm::expressions::Expression>> mParameters;
};

} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org