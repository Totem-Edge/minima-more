#pragma once

#include <cstddef>

namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace expressions {

class Expression {
public:
    virtual ~Expression();

    // May throw org::minima::kissvm::exceptions::ExecutionException
    virtual org::minima::kissvm::values::Value* getValue(org::minima::kissvm::Contract& zContract) = 0;
};

} // namespace expressions
} // namespace kissvm
} // namespace minima
} // namespace org