// Port of uniffi-rs v0.31.2 fixtures/regressions/wrong-lower-check/tests/bindings/test.py
// (The type errors the Python test checks for are compile errors in C++.)
#include "check.hpp"
#include "regression_test_wrong_lower_check.hpp"

using namespace regression_test_wrong_lower_check;

int main() {
    CHECK(!optional_string().has_value());
    CHECK(optional_string("value") == std::optional<std::string>("value"));
    CHECK(!Klass::new_()->optional_string().has_value());
    CHECK(Klass::new_()->optional_string("value") == std::optional<std::string>("value"));
    return check::report();
}
