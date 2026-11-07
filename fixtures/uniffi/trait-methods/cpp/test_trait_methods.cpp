// Port of uniffi-rs v0.31.2 fixtures/trait-methods/tests/bindings/test.py
#include <sstream>
#include <unordered_set>

#include "check.hpp"
#include "trait_methods.hpp"

using namespace trait_methods;

template <class Obj>
void object_traits(const char* name) {
    auto m = Obj::new_("yo");
    CHECK_EQ(m->to_string(), std::string(name) + "(yo)");
    CHECK_EQ(m->debug_string(), std::string(name) + " { val: \"yo\" }");
    std::ostringstream printed;
    printed << *m;
    CHECK_EQ(printed.str(), m->to_string());
    CHECK(*m == *Obj::new_("yo"));
    CHECK(*m != *Obj::new_("yoyo"));
    CHECK_EQ(Obj::new_("m")->hash(), Obj::new_("m")->hash());
    auto a = Obj::new_("a");
    auto b = Obj::new_("b");
    CHECK(*a < *b);
    CHECK(*a <= *b);
    CHECK(*a <= *a);
    CHECK(*b > *a);
    CHECK(*b >= *a);
    CHECK(*b >= *b);
}

int main() {
    object_traits<TraitMethods>("TraitMethods");
    object_traits<ProcTraitMethods>("ProcTraitMethods");

    // Records: Debug, Eq and Hash only look at `s`, as the Rust impls do.
    for (int udl = 0; udl < 2; ++udl) {
        if (udl == 0) {
            TraitRecord r{"yo", 2};
            CHECK_EQ(r.debug_string(), "TraitRecord { s: \"yo\", i: 2 }");
            CHECK((TraitRecord{"yo", 2} == TraitRecord{"yo", 3}));
            CHECK((TraitRecord{"yo", 2} != TraitRecord{"hi", 2}));
            std::unordered_set<TraitRecord> set{TraitRecord{"yo", 2}};
            CHECK(set.count(TraitRecord{"yo", 3}) == 1);
            CHECK((TraitRecord{"a", 0} < TraitRecord{"b", 0}));
        } else {
            UdlRecord r{"yo", 2};
            CHECK_EQ(r.debug_string(), "UdlRecord { s: \"yo\", i: 2 }");
            CHECK((UdlRecord{"yo", 2} == UdlRecord{"yo", 3}));
            CHECK((UdlRecord{"yo", 2} != UdlRecord{"hi", 3}));
            std::unordered_set<UdlRecord> set{UdlRecord{"yo", 2}};
            CHECK(set.count(UdlRecord{"yo", 3}) == 1);
            CHECK((UdlRecord{"a", 0} < UdlRecord{"b", 0}));
        }
    }

    // Enums: equality only compares the variant.
    TraitEnum s_yo{TraitEnum::S{"yo"}};
    CHECK_EQ(s_yo.to_string(), "TraitEnum::S(\"yo\")");
    CHECK_EQ(s_yo.debug_string(), "S(\"yo\")");
    CHECK((TraitEnum{TraitEnum::S{"1"}} == TraitEnum{TraitEnum::S{"2"}}));
    CHECK((TraitEnum{TraitEnum::I{1}} == TraitEnum{TraitEnum::I{2}}));
    CHECK((TraitEnum{TraitEnum::S{"1"}} != TraitEnum{TraitEnum::I{1}}));
    std::unordered_set<TraitEnum> enums{TraitEnum{TraitEnum::S{"m"}}};
    CHECK(enums.count(TraitEnum{TraitEnum::S{"m"}}) == 1);
    CHECK((TraitEnum{TraitEnum::S{"1"}} < TraitEnum{TraitEnum::I{1}}));
    CHECK((TraitEnum{TraitEnum::S{"1"}} <= TraitEnum{TraitEnum::I{1}}));

    CHECK_EQ((UdlEnum{UdlEnum::S{"yo"}}.debug_string()), "S { s: \"yo\" }");
    CHECK((UdlEnum{UdlEnum::S{"1"}} == UdlEnum{UdlEnum::S{"2"}}));
    CHECK((UdlEnum{UdlEnum::I{1}} == UdlEnum{UdlEnum::I{2}}));
    CHECK((UdlEnum{UdlEnum::S{"1"}} != UdlEnum{UdlEnum::I{1}}));
    CHECK((UdlEnum{UdlEnum::S{"1"}} < UdlEnum{UdlEnum::I{1}}));
    CHECK((UdlEnum{UdlEnum::S{"1"}} <= UdlEnum{UdlEnum::I{2}}));

    CHECK((XYZEnum{XYZEnum::XYZNone{}} == XYZEnum{XYZEnum::XYZNone{}}));

    // Flat enums get free functions, and operator<< for Display.
    CHECK_EQ(to_string(get_flat_trait_enum(0)), "FlatTraitEnum::flat-alpha");
    CHECK_EQ(to_string(FlatTraitEnum::Gamma), "FlatTraitEnum::flat-gamma");
    std::ostringstream flat;
    flat << FlatTraitEnum::Beta;
    CHECK_EQ(flat.str(), "FlatTraitEnum::flat-beta");

    // Errors with traits: what() is their Display text.
    CHECK_THROWS_AS(throw_flat_error(1), FlatError_Unauthorized,
                    CHECK_EQ(std::string(e.what()), "error: unauthorized"));
    CHECK_THROWS_AS(throw_api_failure(0), ApiFailure, {
        CHECK_EQ(std::string(e.what()), "api network issue");
        CHECK_EQ(e.debug_string(), "NetworkIssue");
        CHECK(e == ApiFailure_NetworkIssue());
        CHECK(e < ApiFailure_ServerDown());
        CHECK_EQ(e.hash(), ApiFailure_NetworkIssue().hash());
    });
    CHECK_THROWS_AS(throw_multiple_trait_error(1), MultipleTraitError_Nested, {
        CHECK_EQ(std::string(e.what()), "nested error: error: not found");
        CHECK(e > MultipleTraitError_NoData());
    });
    return check::report();
}
