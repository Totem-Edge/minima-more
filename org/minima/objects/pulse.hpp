#pragma once

#include <vector>
#include <istream>
#include <ostream>

#include "org/minima/utils/streamable.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

namespace org {
namespace minima {
namespace objects {

class Pulse : public org::minima::utils::Streamable {
public:
    // Static version (matches Java: MiniNumber.ONE)
    static org::minima::objects::base::MiniNumber PULSE_VERSION;

    Pulse();

    void setBlockList(const std::vector<org::minima::objects::base::MiniData>& zBlockList);
    const std::vector<org::minima::objects::base::MiniData>& getBlockList() const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Static helpers
    static Pulse ReadFromStream(std::istream& in);

    // Create a PULSE message to help peers keep in sync
    static Pulse createPulse();

private:
    // Latest block hashes (not all - just the last 60 minutes)
    std::vector<org::minima::objects::base::MiniData> mBlockList;
};

} // namespace objects
} // namespace minima
} // namespace org