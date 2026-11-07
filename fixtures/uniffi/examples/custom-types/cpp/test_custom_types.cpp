// Port of uniffi-rs v0.31.2 examples/custom-types/tests/bindings/test_custom_types.py
// The C++ section of uniffi.toml maps `TimeIntervalMs` to a std::chrono time point.
#include "check.hpp"
#include "custom_types.hpp"

using namespace custom_types;

int main() {
    auto val = get_custom_types_demo(std::nullopt);
    CHECK_EQ(val.url, "http://example.com/");
    CHECK_EQ(val.handle, 123);
    CHECK(val.time_interval_ms.time_since_epoch() == std::chrono::milliseconds(456000));

    val.url = "http://new.example.com/";
    val.handle = 456;
    val.time_interval_ms = std::chrono::system_clock::time_point(std::chrono::milliseconds(789));
    auto back = get_custom_types_demo(val);
    CHECK_EQ(back.url, val.url);
    CHECK_EQ(back.handle, 456);
    CHECK(back.time_interval_ms == val.time_interval_ms);
    CHECK(back == val);
    return check::report();
}
