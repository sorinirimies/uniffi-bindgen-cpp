// Port of uniffi-rs v0.32.2 fixtures/regressions/unary-result-alias/tests/bindings/test.py
#include "check.hpp"
#include "unary_result_alias.hpp"

int main() {
    unary_result_alias::returns_unary_result_alias();
    CHECK(true);
    return check::report();
}
