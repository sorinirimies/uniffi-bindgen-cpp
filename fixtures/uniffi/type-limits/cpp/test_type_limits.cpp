// Port of uniffi-rs v0.32.2 fixtures/type-limits/tests/bindings/test_type_limits.py
// Out-of-range and wrongly typed arguments are compile errors in C++, so only the
// limits themselves, special floats, strings and bytes are checked at run time.
#include <cmath>
#include <limits>

#include "check.hpp"
#include "uniffi_type_limits.hpp"

using namespace uniffi_type_limits;

template <class T, class F>
void limits(F take) {
    CHECK_EQ(take(std::numeric_limits<T>::min()), std::numeric_limits<T>::min());
    CHECK_EQ(take(std::numeric_limits<T>::max()), std::numeric_limits<T>::max());
    CHECK_EQ(take(T{0}), T{0});
    CHECK_EQ(take(T{100}), T{100});
}

int main() {
    limits<int8_t>(take_i8);
    limits<int16_t>(take_i16);
    limits<int32_t>(take_i32);
    limits<int64_t>(take_i64);
    limits<uint8_t>(take_u8);
    limits<uint16_t>(take_u16);
    limits<uint32_t>(take_u32);
    limits<uint64_t>(take_u64);
    CHECK_EQ(take_i64(1000000000000000000LL), 1000000000000000000LL);
    CHECK_EQ(take_u64(10000000000000000000ULL), 10000000000000000000ULL);

    const auto inf = std::numeric_limits<double>::infinity();
    CHECK_EQ(take_f32(static_cast<float>(inf)), static_cast<float>(inf));
    CHECK_EQ(take_f64(inf), inf);
    CHECK_EQ(take_f32(static_cast<float>(-inf)), static_cast<float>(-inf));
    CHECK_EQ(take_f64(-inf), -inf);
    CHECK_EQ(std::copysign(1.0f, take_f32(0.0f)), 1.0f);
    CHECK_EQ(std::copysign(1.0, take_f64(0.0)), 1.0);
    CHECK_EQ(std::copysign(1.0f, take_f32(-0.0f)), -1.0f);
    CHECK_EQ(std::copysign(1.0, take_f64(-0.0)), -1.0);
    CHECK(std::isnan(take_f32(std::numeric_limits<float>::quiet_NaN())));
    CHECK(std::isnan(take_f64(std::numeric_limits<double>::quiet_NaN())));
    CHECK_EQ(take_f32(123.0f), 123.0f);

    CHECK_EQ(take_string(""), "");
    CHECK_EQ(take_string("\xE6\x84\x9B"), "\xE6\x84\x9B");
    CHECK_EQ(take_string("\xF0\x9F\x92\x96"), "\xF0\x9F\x92\x96");
    // A C++ string is bytes; Rust only accepts UTF-8 and reports it as an error.
    CHECK_THROWS(take_string("\xED\xA0\x80"), uniffi::Exception);

    using Bytes = std::vector<uint8_t>;
    CHECK(take_bytes({}).empty());
    CHECK(take_bytes({0xff}) == Bytes{0xff});
    CHECK(take_bytes({0xed, 0xa0, 0x80}) == (Bytes{0xed, 0xa0, 0x80}));
    CHECK(take_bytes({0x1b, 0x61}) == (Bytes{0x1b, 0x61}));
    CHECK(take_bytes({0x3d, 0xd8, 0x96, 0xdc}) == (Bytes{0x3d, 0xd8, 0x96, 0xdc}));
    return check::report();
}
