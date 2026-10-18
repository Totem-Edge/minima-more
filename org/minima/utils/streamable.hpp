#pragma once

#include <istream>
#include <ostream>
#include <stdexcept> // For std::runtime_error

namespace org {
namespace minima {
namespace utils {

/**
 * Streamable: Interface for objects that can serialize and deserialize themselves
 * to/from binary streams.
 * ... (rest of comments)
 */
class Streamable {
public:
    // Virtual destructor is necessary for proper cleanup of derived classes
    virtual ~Streamable(); // Use = default in the header

    /**
     * Write in full to the output stream.
     * May throw on I/O error.
     */
    // Declared as pure virtual (= 0) and using standard C++ streams
    virtual void writeDataStream(std::ostream& out) = 0;
    

    /**
     * Read in full from the input stream.
     * May throw on I/O error.
     */
    // Declared as pure virtual (= 0) and using standard C++ streams
    virtual void readDataStream(std::istream& in) = 0;
};

} // namespace utils
} // namespace minima
} // namespace org

// The original implementation was just the default destructor.
// With the pure virtual methods, the destructor should be defined
// as a default definition *in the .cpp file* if you keep the pure virtual methods.
// The current streamable.cpp is correct for a class with a pure virtual destructor.