#pragma once

#include <memory>

namespace org {
namespace minima {
namespace utils {
namespace json {
namespace parser {

/**
 * Container factory for creating containers for JSON object and JSON array.
 *
 * Semantics:
 * - Return a non-null pointer to provide a custom container instance for objects/arrays.
 * - Return nullptr to signal the parser to use its default container types.
 *
 * Note:
 * - The returned pointers are type-erased (std::shared_ptr<void>). The JSON parser
 *   will downcast to expected container types according to project conventions.
 * - Method name 'creatArrayContainer' intentionally matches the original Java API.
 */
class ContainerFactory {
public:
    virtual ~ContainerFactory() noexcept;

    /**
     * @return A pointer to an object container instance (Map-like), or nullptr to use the default.
     */
    virtual std::shared_ptr<void> createObjectContainer() = 0;

    /**
     * @return A pointer to an array container instance (List-like), or nullptr to use the default.
     * Note the method name matches the original Java API ('creatArrayContainer').
     */
    virtual std::shared_ptr<void> creatArrayContainer() = 0;
};

} // namespace parser
} // namespace json
} // namespace utils
} // namespace minima
} // namespace org