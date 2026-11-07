// Port of uniffi-rs v0.32.2 examples/futures/tests/bindings/test.py
#include "check.hpp"
#include "uniffi_example_futures.hpp"

int main() {
    CHECK_EQ(uniffi_example_futures::say_after(20, "Alice").get(), "Hello, Alice!");
    return check::report();
}
