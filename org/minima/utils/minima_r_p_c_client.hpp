#pragma once

#include <string>

namespace org {
namespace minima {
namespace utils {

class MinimaRPCClient {
public:
    // Runs the Minima RPC Client. Returns an OS exit code.
    static int run(int argc, char* argv[]);
};

} // namespace utils
} // namespace minima
} // namespace org