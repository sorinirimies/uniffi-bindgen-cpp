// Port of uniffi-rs v0.32.2 examples/arithmetic/tests/bindings/test_arithmetic.py
#include "arithmetic.hpp"
#include "check.hpp"

using namespace arithmetic;

int main() {
    CHECK_THROWS(add(18446744073709551615ULL, 1), ArithmeticError_IntegerOverflow);
    CHECK_EQ(add(2, 4), 6u);
    CHECK_EQ(add(4, 8), 12u);
    CHECK_THROWS(sub(0, 1), ArithmeticError_IntegerOverflow);
    CHECK_EQ(sub(4, 2), 2u);
    CHECK_EQ(sub(8, 4), 4u);
    CHECK_EQ(arithmetic::div(8, 4), 2u);
    // Dividing by zero panics in Rust.
    CHECK_THROWS(arithmetic::div(8, 0), uniffi::PanicError);
    CHECK(equal(2, 2));
    CHECK(equal(4, 4));
    CHECK(!equal(2, 4));
    CHECK(!equal(4, 8));
    return check::report();
}
