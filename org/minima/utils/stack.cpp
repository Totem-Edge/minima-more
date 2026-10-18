#include "org/minima/utils/stack.hpp"

#include <utility>

namespace org {
namespace minima {
namespace utils {

Stack::Stack() : mStack() {}

void Stack::push(const std::any& zObject) {
    mStack.push_back(zObject);
}

void Stack::push(std::any&& zObject) {
    mStack.push_back(std::move(zObject));
}

std::any Stack::pop() {
    if (isEmpty()) {
        return std::any{};
    }
    std::any top = std::move(mStack.back());
    mStack.pop_back();
    return top;
}

std::any Stack::peek() const {
    if (isEmpty()) {
        return std::any{};
    }
    // Return a copy; if contained type is a pointer/smart pointer, reference-like semantics are preserved.
    return mStack.back();
}

bool Stack::isEmpty() const {
    return mStack.empty();
}

} // namespace utils
} // namespace minima
} // namespace org