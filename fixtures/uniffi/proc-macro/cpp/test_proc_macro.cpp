// Port of uniffi-rs v0.31.2 fixtures/proc-macro/tests/bindings/test_proc_macro.py
#include <stdexcept>

#include "check.hpp"
#include "proc_macro.hpp"

using namespace proc_macro;

class CppCallback2 : public OtherCallbackInterface {
public:
    uint32_t multiply(uint32_t a, uint32_t b) override { return a * b; }
};

class CppCallback : public TestCallbackInterface {
public:
    void do_nothing() override {}
    uint32_t add(uint32_t a, uint32_t b) override { return a + b; }
    uint32_t optional(const std::optional<uint32_t>& a) override { return a.value_or(0); }
    std::vector<uint8_t> with_bytes(const RecordWithBytes& rwb) override { return rwb.some_bytes; }
    uint32_t try_parse_int(const std::string& value) override {
        if (value == "force-unexpected-error") {
            throw std::out_of_range(value);
        }
        try {
            return static_cast<uint32_t>(std::stoul(value));
        } catch (const std::exception&) {
            throw BasicError_InvalidInput();
        }
    }
    uint32_t callback_handler(const std::shared_ptr<Object>& h) override {
        return h->take_error(std::make_shared<BasicError_InvalidInput>());
    }
    std::shared_ptr<OtherCallbackInterface> get_other_callback_interface() override {
        return std::make_shared<CppCallback2>();
    }
};

int main() {
    auto one = make_one(123);
    CHECK_EQ(one.inner, 123);
    CHECK_EQ(one_inner_by_ref(one), 123);
    CHECK_EQ(one.get_inner_value(), 123);

    CHECK_EQ(take_two(Two{"a"}), "a");
    RecordWithBytes rwb{{1, 2, 3}};
    CHECK(take_record_with_bytes(rwb) == (std::vector<uint8_t>{1, 2, 3}));

    auto obj = Object::named_ctor(1);
    CHECK(obj->is_heavy() == MaybeBool::Uncertain);
    auto obj2 = Object::new_();
    CHECK(obj->is_other_heavy(obj2) == MaybeBool::Uncertain);

    auto robj = Renamed::new_();
    CHECK(robj->func());
    CHECK(rename_test());

    auto trait_impl = obj->get_trait(std::nullopt);
    CHECK_EQ(trait_impl->concat_strings("foo", "bar"), "foobar");
    CHECK_EQ(obj->get_trait(trait_impl)->concat_strings("foo", "bar"), "foobar");
    CHECK_EQ(concat_strings_by_ref(trait_impl, "foo", "bar"), "foobar");
    CHECK_EQ(StructWithTrait::new_("me")->concat_strings("foo", "bar"), "me: foobar");

    auto trait_impl2 = obj->get_trait_with_foreign(std::nullopt);
    CHECK_EQ(trait_impl2->name(), "RustTraitImpl");
    CHECK_EQ(obj->get_trait_with_foreign(trait_impl2)->name(), "RustTraitImpl");

    CHECK(enum_identity(MaybeBool::True) == MaybeBool::True);
    Three three{obj};
    CHECK(three.obj == obj);
    CHECK_EQ(make_zero().inner, "ZERO");
    CHECK(make_record_with_bytes().some_bytes == (std::vector<uint8_t>{0, 1, 2, 3, 4}));
    CHECK((make_hashmap(1, 2) == std::map<int8_t, uint64_t>{{1, 2}}));
    std::map<int8_t, uint64_t> d{{1, 2}};
    CHECK(return_hashmap(d) == d);
    CHECK_EQ(join({"a", "b", "c"}, ":"), "a:b:c");

    // A method on an error enum.
    CHECK_THROWS_AS(always_fails(), BasicError_OsError, CHECK(!e.is_unexpected()));
    obj->do_stuff(5);
    CHECK_THROWS(obj->do_stuff(0), FlatError_InvalidInput);

    // Record defaults.
    RecordWithDefaults with_defaults;
    with_defaults.no_default_string = "Test";
    CHECK_EQ(with_defaults.no_default_string, "Test");
    CHECK(with_defaults.boolean);
    CHECK_EQ(with_defaults.integer, 42);
    CHECK_EQ(with_defaults.float_var, 4.2);
    CHECK(!with_defaults.opt_vec.has_value());
    CHECK(with_defaults.opt_integer == std::optional<int32_t>(42));
    CHECK_EQ(with_defaults.custom_integer, 42);

    RecordWithImplicitDefaults implicit;
    CHECK(!implicit.boolean);
    CHECK_EQ(implicit.int8, 0);
    CHECK_EQ(implicit.uint64, 0u);
    CHECK_EQ(implicit.afloat, 0.0f);
    CHECK_EQ(implicit.adouble, 0.0);
    CHECK(implicit.vec.empty());
    CHECK(implicit.map.empty());
    CHECK(implicit.some_bytes.empty());
    CHECK(!implicit.opt_int32.has_value());
    CHECK_EQ(implicit.custom_integer, 0);

    // Default arguments.
    CHECK_EQ(double_with_default(), 42);
    CHECK_EQ(sum_with_default(1), 1);
    CHECK_EQ(sum_with_default(1, 2), 3);
    auto with_defaults_obj = ObjectWithDefaults::new_();
    CHECK_EQ(with_defaults_obj->add_to_num(), 42);
    CHECK_EQ(with_defaults_obj->add_to_implicit_num(), 30);
    CHECK_EQ(with_defaults_obj->add_to_implicit_num(1), 31);

    // Rust drives the callback interface through every case, including errors.
    call_callback_interface(std::make_shared<CppCallback>());

    CHECK_EQ(get_one(std::nullopt).inner, 0);
    CHECK(get_bool(std::nullopt) == MaybeBool::Uncertain);
    CHECK(get_object(std::nullopt)->is_heavy() == MaybeBool::Uncertain);
    CHECK_EQ(get_trait_with_foreign(std::nullopt)->name(), "RustTraitImpl");
    CHECK(!get_externals(std::nullopt).one.has_value());

    CHECK_EQ(static_cast<int32_t>(MaybeBool::True), 0);
    CHECK_EQ(static_cast<int32_t>(MaybeBool::Uncertain), 2);
    CHECK_EQ(static_cast<uint8_t>(ReprU8::One), 1);
    CHECK_EQ(static_cast<uint8_t>(ReprU8::Three), 3);

    using M = MixedEnum;
    CHECK(get_mixed_enum(std::nullopt) == M{M::Int{1}});
    CHECK(get_mixed_enum(M{M::None{}}) == M{M::None{}});
    CHECK(get_mixed_enum(M{M::String{"hello"}}) == M{M::String{"hello"}});
    M both{M::Both{"hello", 1}};
    CHECK_EQ(both.get_if<M::Both>()->v0, "hello");
    CHECK_EQ(both.get_if<M::Both>()->v1, 1);

    // Methods on enums: free functions for flat enums, members for data enums.
    CHECK(next(get_bool(std::nullopt)) == MaybeBool::True);
    CHECK(next(MaybeBool::True) == MaybeBool::False);
    CHECK((M{M::String{""}}.is_not_none()));
    CHECK(!(M{M::None{}}.is_not_none()));
    CHECK(get_mixed_enum(M{M::All{"string", 2}}).is<M::All>());
    CHECK(get_mixed_enum(M{M::Vec{{"string"}}}).is<M::Vec>());
    return check::report();
}
