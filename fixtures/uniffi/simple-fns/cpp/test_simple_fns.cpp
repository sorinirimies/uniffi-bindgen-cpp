// Port of uniffi-rs v0.31.2 fixtures/simple-fns/tests/bindings/test_simple_fns.py
#include "check.hpp"
#include "uniffi_simple_fns.hpp"

using namespace uniffi_simple_fns;

int main() {
    CHECK_EQ(get_string(), "String created by Rust");
    CHECK_EQ(get_int(), 1289);
    CHECK_EQ(string_identity("String created by C++"), "String created by C++");
    CHECK_EQ(byte_to_u32(255), 255u);

    auto a_set = new_set();
    add_to_set(a_set, "foo");
    add_to_set(a_set, "bar");
    CHECK(set_contains(a_set, "foo"));
    CHECK(set_contains(a_set, "bar"));
    CHECK(!set_contains(a_set, "baz"));

    std::map<std::string, std::string> h{{"a", "b"}};
    CHECK(hash_map_identity(h) == h);
    dummy(std::nullopt);
    return check::report();
}
