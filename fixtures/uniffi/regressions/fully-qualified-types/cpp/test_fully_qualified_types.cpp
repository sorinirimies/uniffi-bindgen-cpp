// uniffi-rs has no binding test for this fixture (it checks that the Rust side
// compiles with shadowed std names); `test()` returns None.
#include "check.hpp"
#include "regression_test_i1015.hpp"

int main() {
    CHECK(!regression_test_i1015::test().has_value());
    return check::report();
}
