// Port of uniffi-rs v0.32.2 fixtures/error-types/tests/bindings/test.py
#include <cstring>

#include "check.hpp"
#include "error_types.hpp"

using namespace error_types;

static const char* const OOPS = "because uniffi told me so\n\nCaused by:\n    oops";

int main() {
    // Objects as errors: what() is the Rust Display text.
    CHECK_THROWS_AS(oops(), ErrorInterface, CHECK_EQ(std::string(e.what()), OOPS));
    CHECK_THROWS_AS(oops_nowrap(), ErrorInterface, CHECK_EQ(std::string(e.what()), OOPS));
    CHECK_THROWS_AS(oops(), ErrorInterface, {
        CHECK(e.chain() == (std::vector<std::string>{"because uniffi told me so", "oops"}));
        CHECK(e.link(0) == std::optional<std::string>("because uniffi told me so"));
        CHECK_EQ(e.debug_string(), "ErrorInterface { e: because uniffi told me so\n\nCaused by:\n    oops }");
        CHECK_EQ(e.to_string(), OOPS);
    });
    // An error caught through its uniffi::Exception base keeps its message.
    CHECK_THROWS_AS(oops(), uniffi::Exception, CHECK_EQ(std::string(e.what()), OOPS));
    CHECK_THROWS_AS(aoops().get(), ErrorInterface, CHECK_EQ(std::string(e.what()), "async-oops"));

    // A trait object as error.
    CHECK_THROWS_AS(toops(), ErrorTrait, CHECK_EQ(const_cast<ErrorTrait&>(e).msg(), "trait-oops"));

    // Functions that return an error instead of throwing it.
    auto returned = get_error("the error");
    CHECK(returned->chain() == std::vector<std::string>{"the error"});
    CHECK_EQ(returned->debug_string(), "ErrorInterface { e: the error }");
    CHECK_EQ(returned->to_string(), "the error");

    CHECK_THROWS_AS(throw_rich("oh no"), RichError, {
        CHECK_EQ(e.debug_string(), "RichError { e: \"oh no\" }");
        CHECK_EQ(e.to_string(), "RichError: \"oh no\"");
    });

    // Constructors and methods of objects throw too.
    CHECK_THROWS_AS(TestInterface::fallible_new(), ErrorInterface, CHECK_EQ(std::string(e.what()), "fallible_new"));
    auto interface = TestInterface::new_();
    CHECK_THROWS_AS(interface->oops(), ErrorInterface,
                    CHECK_EQ(std::string(e.what()), "because the interface told me so\n\nCaused by:\n    oops"));
    CHECK_THROWS_AS(interface->aoops().get(), ErrorInterface, CHECK_EQ(std::string(e.what()), "async-oops"));

    CHECK_THROWS_AS(throw_proc_error("eek"), ProcErrorInterface, {
        CHECK_EQ(e.message(), "eek");
        CHECK_EQ(e.to_string(), "ProcErrorInterface(eek)");
    });

    // Enum errors, with and without fields.
    CHECK_THROWS(oops_enum(0), Error_Oops);
    CHECK_THROWS_AS(oops_enum(1), Error_Value, CHECK_EQ(e.value, "value"));
    CHECK_THROWS_AS(oops_enum(2), Error_IntValue, CHECK_EQ(e.value, 2));
    CHECK_THROWS_AS(oops_enum(3), Error_FlatInnerError, {
        CHECK(std::dynamic_pointer_cast<FlatInner_CaseA>(e.error) != nullptr);
        CHECK_EQ(std::string(e.error->what()), "inner");
    });
    CHECK_THROWS_AS(oops_enum(4), Error_FlatInnerError, {
        CHECK(std::dynamic_pointer_cast<FlatInner_CaseB>(e.error) != nullptr);
        CHECK_EQ(std::string(e.error->what()), "NonUniffiTypeValue: value");
    });
    CHECK_THROWS_AS(oops_enum(5), Error_InnerError, {
        auto inner = std::dynamic_pointer_cast<Inner_CaseA>(e.error);
        CHECK(inner && inner->v0 == "inner");
    });
    // Every variant is also its enum's base class.
    CHECK_THROWS(oops_enum(1), Error);

    // Tuple variants, as values and thrown.
    auto tuple = get_tuple(std::nullopt);
    auto oops_variant = std::dynamic_pointer_cast<TupleError_Oops>(tuple);
    CHECK(oops_variant && oops_variant->v0 == "oops");
    auto round_trip = std::dynamic_pointer_cast<TupleError_Oops>(get_tuple(tuple));
    CHECK(round_trip && round_trip->v0 == "oops");
    CHECK_THROWS_AS(oops_tuple(0), TupleError_Oops, CHECK_EQ(e.v0, "oops"));
    CHECK_THROWS_AS(oops_tuple(1), TupleError_Value, CHECK_EQ(e.v0, 1));
    // A custom type that wraps an error enum is thrown as that enum.
    CHECK_THROWS_AS(oops_custom(1), TupleError_Value, CHECK_EQ(e.v0, 1));
    return check::report();
}
