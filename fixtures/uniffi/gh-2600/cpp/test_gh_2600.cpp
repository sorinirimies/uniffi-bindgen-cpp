// Port of uniffi-rs v0.32.2 fixtures/gh-2600/tests/bindings/test_gh_2600.py
// An object with 32-byte alignment must live until its last handle is dropped.
#include "check.hpp"
#include "gh_2600.hpp"

using namespace gh_2600;

int main() {
    {
        auto obj = MyStruct256::new_();
        CHECK_EQ(drop_count(), 0u);
        obj->method();
        CHECK_EQ(drop_count(), 0u);
        auto copy = obj;
        obj.reset();
        copy->method();
        CHECK_EQ(drop_count(), 0u);
    }
    // The last shared_ptr is gone: Rust dropped the object exactly once.
    CHECK_EQ(drop_count(), 1u);
    return check::report();
}
