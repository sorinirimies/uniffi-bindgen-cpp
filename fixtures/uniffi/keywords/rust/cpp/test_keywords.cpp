// Port of uniffi-rs v0.32.2 fixtures/keywords/rust/tests/bindings/test_keywords.py
// Every name in this fixture is a keyword somewhere; C++ ones get a trailing `_`.
#include "check.hpp"
#include "keywords_rust.hpp"

int main() {
    keywords_rust::if_(0);
    CHECK(!keywords_rust::return_{}.bool_);
    return check::report();
}
