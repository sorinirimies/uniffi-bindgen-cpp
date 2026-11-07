// Minimal test helpers shared by the C++ ports of the uniffi fixture tests.
#pragma once

#include <cmath>
#include <cstdio>
#include <exception>
#include <string>

namespace check {

inline int& failures() {
    static int count = 0;
    return count;
}

inline int& checks() {
    static int count = 0;
    return count;
}

inline void record(bool ok, const char* file, int line, const char* what) {
    ++checks();
    if (!ok) {
        ++failures();
        std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", file, line, what);
    }
}

inline int report() {
    std::printf("%d checks, %d failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}

inline bool near(double a, double b, double epsilon = 1e-6) {
    return std::fabs(a - b) <= epsilon * (1.0 + std::fabs(a) + std::fabs(b));
}

}  // namespace check

// Variadic, so that braced initializers with commas need no extra parentheses.
#define CHECK(...) ::check::record(static_cast<bool>(__VA_ARGS__), __FILE__, __LINE__, #__VA_ARGS__)

#define CHECK_EQ(a, b) ::check::record((a) == (b), __FILE__, __LINE__, #a " == " #b)

// Checks that `expr` throws an exception of type `type`; `then` may inspect it as `e`.
#define CHECK_THROWS_AS(expr, type, then)                                                  \
    do {                                                                                   \
        bool uniffi_check_thrown = false;                                                  \
        try {                                                                              \
            (void)(expr);                                                                  \
        } catch (const type& e) {                                                          \
            uniffi_check_thrown = true;                                                    \
            (void)e;                                                                       \
            then;                                                                          \
        } catch (const std::exception& other) {                                            \
            std::fprintf(stderr, "%s:%d: unexpected exception: %s\n", __FILE__, __LINE__,  \
                         other.what());                                                    \
        }                                                                                  \
        ::check::record(uniffi_check_thrown, __FILE__, __LINE__, #expr " throws " #type); \
    } while (0)

#define CHECK_THROWS(expr, type) CHECK_THROWS_AS(expr, type, (void)0)
