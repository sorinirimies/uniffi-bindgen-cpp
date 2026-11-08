// std::format of the types with a Rust Display (C++20; the bindings specialize std::formatter
// where the standard library has <format>).
#include <string>

#include "check.hpp"
#include "trait_methods.hpp"

using namespace trait_methods;

int main() {
#if defined(__cpp_lib_format)
    auto object = TraitMethods::new_("yo");
    CHECK_EQ(std::format("{}", *object), object->to_string());
    CHECK_EQ(std::format("[{:>18}]", *object), "[  TraitMethods(yo)]");
    TraitEnum data{TraitEnum::S{"yo"}};
    CHECK_EQ(std::format("{}", data), "TraitEnum::S(\"yo\")");
    CHECK_EQ(std::format("{}", FlatTraitEnum::Alpha), to_string(FlatTraitEnum::Alpha));
#else
    CHECK(true);  // no <format> in this standard library
#endif
    return check::report();
}
