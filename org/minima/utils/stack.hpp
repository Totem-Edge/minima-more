#pragma once

#include <any>
#include <vector>

namespace org {
namespace minima {
namespace utils {

class Stack {
public:
    Stack();

    // Add a node to the top of the Stack
    void push(const std::any& zObject);
    void push(std::any&& zObject);

    // Pop a node off the top of the stack; returns empty std::any if empty
    std::any pop();

    // Peek at the top node; returns empty std::any if empty
    std::any peek() const;

    // Empty Stack
    bool isEmpty() const;

private:
    std::vector<std::any> mStack;
};

} // namespace utils
} // namespace minima
} // namespace org