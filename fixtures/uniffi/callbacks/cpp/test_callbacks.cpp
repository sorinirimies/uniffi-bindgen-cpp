// Port of uniffi-rs v0.32.2 fixtures/callbacks/tests/bindings/test_callbacks.py
#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "check.hpp"
#include "fixture_callbacks.hpp"

using namespace fixture_callbacks;

class CppGetters : public ForeignGetters {
public:
    bool get_bool(bool v, bool argument_two) override { return v ^ argument_two; }

    std::string get_string(const std::string& v, bool arg2) override {
        if (v == "bad-argument") {
            throw SimpleError_BadArgument("bad argument");
        }
        if (v == "unexpected-error") {
            throw std::runtime_error("unexpected value");
        }
        return arg2 ? "1234567890123" : v;
    }

    std::optional<std::string> get_option(const std::optional<std::string>& v, bool arg2) override {
        if (v == "bad-argument") {
            throw ComplexError_ReallyBadArgument(20);
        }
        if (v == "unexpected-error") {
            throw std::runtime_error("unexpected value");
        }
        if (arg2 && v) {
            std::string upper = *v;
            std::transform(upper.begin(), upper.end(), upper.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            return upper;
        }
        return arg2 ? std::nullopt : v;
    }

    std::vector<int32_t> get_list(const std::vector<int32_t>& v, bool arg2) override {
        return arg2 ? v : std::vector<int32_t>{};
    }

    void get_nothing(const std::string& v) override {
        if (v == "bad-argument") {
            throw SimpleError_BadArgument("bad argument");
        }
        if (v == "unexpected-error") {
            throw std::runtime_error("unexpected value");
        }
    }
};

class CppStringifier : public StoredForeignStringifier {
public:
    std::string from_simple_type(int32_t value) override { return "c++: " + std::to_string(value); }
    std::string from_complex_type(const std::optional<std::vector<std::optional<double>>>&) override {
        return "c++: complex";
    }
};

int main() {
    auto rust_getters = RustGetters::new_();
    auto callback = std::make_shared<CppGetters>();

    for (bool v : {true, false}) {
        CHECK_EQ(rust_getters->get_bool(callback, v, true), callback->get_bool(v, true));
    }
    for (const auto& v : {std::vector<int32_t>{1, 2}, std::vector<int32_t>{0, 1}}) {
        CHECK(rust_getters->get_list(callback, v, true) == callback->get_list(v, true));
    }
    for (const char* v : {"Hello", "world"}) {
        CHECK_EQ(rust_getters->get_string(callback, v, true), callback->get_string(v, true));
    }
    for (const auto& v : {std::optional<std::string>("Some"), std::optional<std::string>()}) {
        CHECK(rust_getters->get_option(callback, v, false) == callback->get_option(v, false));
    }
    CHECK(rust_getters->get_string_optional_callback(callback, "TestString", false) ==
          std::optional<std::string>("TestString"));
    CHECK(!rust_getters->get_string_optional_callback(std::nullopt, "TestString", false).has_value());
    rust_getters->get_nothing(callback, "TestString");

    // A callback stored in a Rust object.
    auto stringifier = std::make_shared<CppStringifier>();
    auto rust_stringifier = RustStringifier::new_(stringifier);
    for (int32_t v : {1, 2}) {
        CHECK_EQ(rust_stringifier->from_simple_type(v), stringifier->from_simple_type(v));
    }

    // Errors thrown by callbacks reach Rust and come back.
    CHECK_THROWS(rust_getters->get_string(callback, "bad-argument", true), SimpleError_BadArgument);
    CHECK_THROWS(rust_getters->get_string(callback, "unexpected-error", true), SimpleError_UnexpectedError);
    CHECK_THROWS(rust_getters->get_nothing(callback, "bad-argument"), SimpleError_BadArgument);
    CHECK_THROWS(rust_getters->get_nothing(callback, "unexpected-error"), SimpleError_UnexpectedError);
    CHECK_THROWS_AS(rust_getters->get_option(callback, "bad-argument", true), ComplexError_ReallyBadArgument,
                    CHECK_EQ(e.code, 20));
    CHECK_THROWS_AS(rust_getters->get_option(callback, "unexpected-error", true),
                    ComplexError_UnexpectedErrorWithReason, CHECK_EQ(e.reason, "unexpected value"));

    // Dropping one Rust reference to a callback keeps the others working.
    auto first = RustStringifier::new_(stringifier);
    {
        auto second = RustStringifier::new_(stringifier);
        CHECK_EQ(second->from_simple_type(123), "c++: 123");
    }
    CHECK_EQ(first->from_simple_type(123), "c++: 123");
    return check::report();
}
