// Port of uniffi-rs v0.32.2 fixtures/regressions/missing-newline/tests/bindings/test.py
#include "check.hpp"
#include "regression_test_missing_newline.hpp"

using namespace regression_test_missing_newline;

int main() {
    empty_func();
    CHECK(get_dict().empty());
    return check::report();
}
