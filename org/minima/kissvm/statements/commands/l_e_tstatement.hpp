#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/kissvm/statements/statement.hpp"

// Forward declarations (namespaced) to avoid heavy includes in header
namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace expressions { class Expression; } } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace statements {
namespace commands {

class LETstatement final : public org::minima::kissvm::statements::Statement {
public:
    // Types of LET statement
    static constexpr int LET_VARIABLE = 0;
    static constexpr int LET_ARRAY    = 1;

    // VARIABLE constructor
    LETstatement(const std::string& zVariableName,
                 std::unique_ptr<org::minima::kissvm::expressions::Expression> zExpression);

    // ARRAY constructor
    LETstatement(std::vector<std::unique_ptr<org::minima::kissvm::expressions::Expression>> zArrayPos,
                 std::unique_ptr<org::minima::kissvm::expressions::Expression> zExpression);

    // Destructor and move operations due to unique_ptr to forward-declared types (PITFALL 1)
    ~LETstatement() override;
    LETstatement(LETstatement&&) noexcept;
    LETstatement& operator=(LETstatement&&) noexcept;

    // Delete copy operations
    LETstatement(const LETstatement&) = delete;
    LETstatement& operator=(const LETstatement&) = delete;

    // Execute the statement within the provided Contract environment.
    void execute(org::minima::kissvm::Contract& zContract) override;

    // String representation (logging/tracing)
    std::string toString() const override;

private:
    int mLETType = LET_VARIABLE;

    // Array index expressions (only used for LET_ARRAY)
    std::vector<std::unique_ptr<org::minima::kissvm::expressions::Expression>> mArrayPos;

    // Variable name (only used for LET_VARIABLE)
    std::string mName;

    // The value expression
    std::unique_ptr<org::minima::kissvm::expressions::Expression> mValue;
};

} // namespace commands
} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org