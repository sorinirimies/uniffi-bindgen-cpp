// Port of uniffi-rs v0.31.2 examples/arithmetic-procmacro/tests/test.py
#include "arithmeticpm.hpp"
#include "check.hpp"

using namespace arithmeticpm;

int main() {
    CHECK_THROWS_AS(add(18446744073709551615ULL, 1), ArithmeticError_IntegerOverflow, {
        CHECK_EQ(e.a, 18446744073709551615ULL);
        CHECK_EQ(e.b, 1u);
    });
    CHECK_EQ(add(2, 4), 6u);
    CHECK_EQ(sub(4, 2), 2u);
    CHECK_EQ(arithmeticpm::div(8, 4), 2u);
    CHECK(equal(2, 2));
    CHECK(!equal(4, 8));
    return check::report();
}
