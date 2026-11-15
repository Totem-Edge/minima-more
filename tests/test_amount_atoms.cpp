// Regression: MiniNumber::getAsLong() returns only the integer part, which
// made a 0.8 MIN balance index as 0 and every sub-1 token balance as 0.
#include "analytics/amount.hpp"

#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(const std::string& what, const std::string& got, const std::string& want) {
    if (got == want) return;
    std::cerr << "FAIL " << what << ": got [" << got << "] want [" << want << "]\n";
    ++failures;
}

std::string atomString(const std::string& value) {
    return minima::analytics::amountAtomsString(
        org::minima::objects::base::MiniNumber(value));
}

std::string formatted(const std::string& value) {
    return minima::analytics::formatAtoms(
        minima::analytics::amountAtoms(
            org::minima::objects::base::MiniNumber(value)));
}

} // namespace

int main() {
    // The address from the live NO MATCH report holds 0.8 MIN.
    expect("0.8 atoms", atomString("0.8"),
           "80000000000000000000000000000000000000000000");
    expect("0.8 format", formatted("0.8"), "0.8");

    expect("1 atom", atomString("1"),
           "100000000000000000000000000000000000000000000");
    expect("1 format", formatted("1"), "1");

    expect("zero", atomString("0"), "0");
    expect("zero format", formatted("0"), "0");

    // One base unit is 10^-44.
    expect("base unit", atomString("0.00000000000000000000000000000000000000000001"), "1");
    expect("base unit format",
           formatted("0.00000000000000000000000000000000000000000001"),
           "0.00000000000000000000000000000000000000000001");

    // A whole-coin balance is unchanged by the conversion.
    expect("whole atoms", atomString("171892925"),
           "17189292500000000000000000000000000000000000000000000");
    expect("whole format", formatted("171892925"), "171892925");

    // Token amounts are small in raw units and used to index as 0.
    expect("token atoms", atomString("99000000000E-44"), "99000000000");
    expect("token format", formatted("99000000000E-44"),
           "0.00000000000000000000000000000000099");

    if (failures != 0) {
        std::cerr << failures << " amount conversion check(s) failed\n";
        return 1;
    }
    std::cout << "amount atoms ok\n";
    return 0;
}
