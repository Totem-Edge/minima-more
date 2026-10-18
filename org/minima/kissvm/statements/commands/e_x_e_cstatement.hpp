#pragma once

#include <memory>
#include <string>
#include "org/minima/kissvm/statements/statement.hpp"

// Forward declarations (Rule 10)
namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace expressions { class Expression; } } } }
namespace org { namespace minima { namespace kissvm { namespace exceptions { class ExecutionException; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace statements {
namespace commands {

class EXECstatement : public org::minima::kissvm::statements::Statement {
private:
    std::unique_ptr<org::minima::kissvm::expressions::Expression> mScript;

public:
    explicit EXECstatement(std::unique_ptr<org::minima::kissvm::expressions::Expression> zScript);

    // Required due to unique_ptr to forward-declared type (Pitfall 1)
    virtual ~EXECstatement();
    EXECstatement(EXECstatement&&) noexcept;
    EXECstatement& operator=(EXECstatement&&) noexcept;

    EXECstatement(const EXECstatement&) = delete;
    EXECstatement& operator=(const EXECstatement&) = delete;

    // Execute the statement; may throw ExecutionException
    void execute(org::minima::kissvm::Contract& zContract) override;

    // String representation
    std::string toString() const override;
};

} // namespace commands
} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org