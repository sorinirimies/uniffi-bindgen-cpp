// Port of uniffi-rs v0.31.2 fixtures/regressions/enum-without-i32-helpers/tests/bindings/test.py
#include "check.hpp"
#include "regression_test_i356.hpp"

using namespace regression_test_i356;

int main() {
    CHECK(which(true) == Which::Yeah);
    CHECK(which(false) == Which::Nah);
    return check::report();
}
