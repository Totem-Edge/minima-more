// Sync-IBD/cascade adoption must be decided by cumulative chain weight, not by
// block height. A peer cascade may replace the local one only when strictly
// heavier; an equal or lighter cascade is ignored.
#include "org/minima/database/cascade/cascade.hpp"

#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(bool got, bool want, const char* name) {
    if (got == want) return;
    std::cerr << "FAIL " << name << ": got " << (got ? "adopt" : "ignore")
              << " want " << (want ? "adopt" : "ignore") << "\n";
    ++failures;
}

} // namespace

int main() {
    using org::minima::objects::base::MiniNumber;
    using org::minima::database::cascade::Cascade;

    // No local cascade: the caller has already checked tip/root linkage, so
    // the weight gate accepts the peer chain.
    expect(Cascade::shouldAdopt(MiniNumber::ZERO(), MiniNumber(1), false), true,
           "empty local accepts peer");
    expect(Cascade::shouldAdopt(MiniNumber::ZERO(), MiniNumber::ZERO(), false), true,
           "empty local accepts zero-weight peer");

    // Strictly heavier incoming cascade is adopted.
    expect(Cascade::shouldAdopt(MiniNumber(10), MiniNumber(11), true), true,
           "heavier incoming adopted");

    // Equal weight is a different chain with the same cumulative weight. Keep
    // the local one; do not reorg for a tie.
    expect(Cascade::shouldAdopt(MiniNumber(10), MiniNumber(10), true), false,
           "equal weight ignored");

    // Lighter incoming cascade must never replace the local chain.
    expect(Cascade::shouldAdopt(MiniNumber(10), MiniNumber(9), true), false,
           "lighter incoming ignored");

    // Cascade weights are wide decimals (block weight * 2^level, 7 significant
    // digits) and can exceed the normal MiniNumber 2^64 bound. Compare them
    // through the wide path without truncation.
    expect(Cascade::shouldAdopt(MiniNumber::fromWideString("100000000000000000000"),
                                MiniNumber::fromWideString("100000000000000000001"), true),
           true, "wide heavier adopted");
    expect(Cascade::shouldAdopt(MiniNumber::fromWideString("100000000000000000001"),
                                MiniNumber::fromWideString("100000000000000000000"), true),
           false, "wide lighter ignored");

    if (failures != 0) {
        std::cerr << failures << " cascade weight check(s) failed\n";
        return 1;
    }
    std::cout << "cascade weight gate ok\n";
    return 0;
}
