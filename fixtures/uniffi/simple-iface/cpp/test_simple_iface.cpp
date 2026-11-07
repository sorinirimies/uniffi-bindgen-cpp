// Port of uniffi-rs v0.32.2 fixtures/simple-iface/tests/bindings/test_simple_iface.py
#include "check.hpp"
#include "uniffi_simple_iface.hpp"

int main() {
    auto obj = uniffi_simple_iface::make_object(9000);
    CHECK_EQ(obj->get_inner(), 9000);
    obj->some_method();
    return check::report();
}
