#pragma once

#include <string>
#include <vector>

// ### FIX 1a: Removed global forward declaration ###
// class Main; // Global forward declaration used by Minima interfaces

// ### FIX 1b: Add namespaced forward declaration instead (optional but good practice) ###
namespace org { namespace minima { namespace system {
class Main;
} } }


namespace org {
namespace minima {

class Minima {
public:
    Minima();

    void mainStarter(const std::vector<std::string>& args);

    // ### FIX 1c: Use fully qualified name for return type ###
    static org::minima::system::Main* getMain();

    std::string runMinimaCMD(const std::string& input);
    std::string runMinimaCMD(const std::string& input, bool prettyJSON);

    static void main(const std::vector<std::string>& args);

private:
    static bool sIsRunning;
};

} // namespace minima
} // namespace org