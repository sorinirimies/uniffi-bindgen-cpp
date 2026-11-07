// Port of uniffi-rs v0.32.2 fixtures/coverall/tests/bindings/test_coverall.py
#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>

#include "check.hpp"
#include "coverall.hpp"

using namespace coverall;

static std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

static bool starts_with(const std::string& s, const std::string& prefix) {
    return s.compare(0, prefix.size(), prefix) == 0;
}

class CppGetters : public Getters {
public:
    bool get_bool(bool v, bool arg2) override { return v ^ arg2; }
    std::string get_string(const std::string& v, bool arg2) override {
        if (v == "too-many-holes") {
            throw CoverallError_TooManyHoles("too many holes");
        }
        if (v == "unexpected-error") {
            throw std::runtime_error("unexpected error");
        }
        return arg2 ? upper(v) : v;
    }
    std::optional<std::string> get_option(const std::string& v, bool arg2) override {
        if (v == "os-error") {
            throw ComplexError_OsError(100, 200);
        }
        if (v == "unknown-error") {
            throw ComplexError_UnknownError();
        }
        if (arg2) {
            return v.empty() ? std::nullopt : std::optional<std::string>(upper(v));
        }
        return v;
    }
    std::vector<int32_t> get_list(const std::vector<int32_t>& v, bool arg2) override {
        return arg2 ? v : std::vector<int32_t>{};
    }
    void get_nothing(const std::string&) override {}
    std::shared_ptr<Coveralls> round_trip_object(const std::shared_ptr<Coveralls>& coveralls) override {
        return coveralls;
    }
};

class CppNode : public NodeTrait {
public:
    std::string name() override { return "node-cpp"; }
    void set_parent(const std::optional<std::shared_ptr<NodeTrait>>& parent) override { parent_ = parent; }
    std::optional<std::shared_ptr<NodeTrait>> get_parent() override { return parent_; }
    uint64_t strong_count() override { return 0; }

private:
    std::optional<std::shared_ptr<NodeTrait>> parent_;
};

static void some_dict() {
    {
        auto d = create_some_dict();
        CHECK_EQ(d.text, "text");
        CHECK(d.maybe_text == std::optional<std::string>("maybe_text"));
        CHECK(d.some_bytes == std::vector<uint8_t>({'s', 'o', 'm', 'e', '_', 'b', 'y', 't', 'e', 's'}));
        CHECK(d.maybe_some_bytes.has_value() && d.maybe_some_bytes->size() == 16);
        CHECK(d.a_bool);
        CHECK(d.maybe_a_bool == std::optional<bool>(false));
        CHECK_EQ(d.unsigned8, 1);
        CHECK(d.maybe_unsigned8 == std::optional<uint8_t>(2));
        CHECK_EQ(d.unsigned16, 3);
        CHECK(d.maybe_unsigned16 == std::optional<uint16_t>(4));
        CHECK_EQ(d.unsigned64, 18446744073709551615ULL);
        CHECK(d.maybe_unsigned64 == std::optional<uint64_t>(0));
        CHECK_EQ(d.signed8, 8);
        CHECK(d.maybe_signed8 == std::optional<int8_t>(0));
        CHECK_EQ(d.signed64, 9223372036854775807LL);
        CHECK(d.maybe_signed64 == std::optional<int64_t>(0));
        CHECK_EQ((*d.coveralls)->get_name(), "some_dict");
        CHECK_EQ((*d.test_trait)->name(), "node-2");
        CHECK(check::near(d.float32, 1.2345f));
        CHECK(check::near(*d.maybe_float32, 22.0f / 7.0f));
        CHECK_EQ(d.float64, 0.0);
        CHECK_EQ(*d.maybe_float64, 1.0);
        CHECK_EQ((*d.coveralls_list[0])->get_name(), "some_dict_1");
        CHECK(!d.coveralls_list[1].has_value());
        CHECK_EQ((*d.coveralls_list[2])->get_name(), "some_dict_2");
        CHECK_EQ((*d.coveralls_map["some_dict_3"])->get_name(), "some_dict_3");
        CHECK(!d.coveralls_map["none"].has_value());
        CHECK_EQ((*d.coveralls_map["some_dict_4"])->get_name(), "some_dict_4");
        CHECK_EQ(get_num_alive(), 5u);
    }
    CHECK_EQ(get_num_alive(), 0u);
}

static void none_dict() {
    auto d = create_none_dict();
    CHECK_EQ(d.text, "text");
    CHECK(!d.maybe_text);
    CHECK(!d.maybe_some_bytes);
    CHECK(!d.maybe_a_bool);
    CHECK(!d.maybe_unsigned8);
    CHECK(!d.maybe_unsigned16);
    CHECK(!d.maybe_unsigned64);
    CHECK(!d.maybe_signed8);
    CHECK(!d.maybe_signed64);
    CHECK(!d.maybe_float32);
    CHECK(!d.maybe_float64);
    CHECK(!d.coveralls);
    CHECK(!d.test_trait);
    CHECK_EQ(get_num_alive(), 0u);
}

static void constructors_and_errors() {
    {
        auto coveralls = Coveralls::new_("c1");
        CHECK_EQ(get_num_alive(), 1u);
        CHECK_EQ(coveralls->get_name(), "c1");
        auto coveralls2 = Coveralls::fallible_new("c2", false);
        CHECK_EQ(get_num_alive(), 2u);
        CHECK_EQ(coveralls2->get_name(), "c2");
        CHECK_THROWS(Coveralls::fallible_new("", true), CoverallError_TooManyHoles);
        CHECK_THROWS_AS(Coveralls::panicking_new("expected panic: woe is me"), uniffi::PanicError,
                        CHECK(std::string(e.what()).find("expected panic: woe is me") != std::string::npos));
        coveralls2.reset();
        CHECK_EQ(get_num_alive(), 1u);
    }
    CHECK_EQ(get_num_alive(), 0u);

    auto coveralls = Coveralls::new_("test_errors");
    CHECK_THROWS_AS(coveralls->maybe_throw(true), CoverallError_TooManyHoles,
                    CHECK_EQ(std::string(e.what()), "The coverall has too many holes"));
    CHECK_THROWS_AS(throw_macro_error(), CoverallMacroError_TooManyMacros,
                    CHECK_EQ(e.message, "The coverall has too many macros"));
    CHECK_THROWS(coveralls->maybe_throw_into(true), CoverallError_TooManyHoles);
    CHECK_THROWS(coveralls->panic("expected panic: oh no"), uniffi::PanicError);

    CHECK_THROWS_AS(throw_flat_error(), CoverallFlatError_TooManyVariants,
                    CHECK_EQ(e.message, "Too many variants: 99"));
    CHECK_THROWS_AS(throw_flat_macro_error(), CoverallFlatMacroError_TooManyVariants,
                    CHECK_EQ(e.message, "Too many variants: 88"));
    CHECK_THROWS(throw_rich_error_no_variant_data(), CoverallRichErrorNoVariantData_TooManyPlainVariants);

    CHECK(coveralls->maybe_throw_complex(0));
    CHECK_THROWS_AS(coveralls->maybe_throw_complex(1), ComplexError_OsError, {
        CHECK_EQ(e.code, 10);
        CHECK_EQ(e.extended_code, 20);
    });
    CHECK_THROWS_AS(throw_complex_macro_error(), ComplexMacroError_OsError, {
        CHECK_EQ(e.code, 1);
        CHECK_EQ(e.extended_code, 2);
    });
    CHECK_THROWS_AS(coveralls->maybe_throw_complex(2), ComplexError_PermissionDenied,
                    CHECK_EQ(e.reason, "Forbidden"));
    CHECK_THROWS(coveralls->maybe_throw_complex(3), ComplexError_UnknownError);
    CHECK_THROWS(coveralls->maybe_throw_complex(4), uniffi::PanicError);

    // Errors as values.
    CHECK_THROWS_AS(throw_root_error(), RootError_Complex, {
        auto os = std::dynamic_pointer_cast<ComplexError_OsError>(e.error);
        CHECK(os && os->code == 1);
    });
    auto root = std::dynamic_pointer_cast<RootError_Other>(get_root_error());
    CHECK(root && root->error == OtherError::Unexpected);
    CHECK(std::dynamic_pointer_cast<ComplexError_PermissionDenied>(get_complex_error(std::nullopt)) != nullptr);
    CHECK(!get_error_dict(std::nullopt).complex_error.has_value());
    CHECK_THROWS(validate_html("test"), HTMLError);
}

static void enums() {
    CHECK(get_simple_flat_macro_enum(0).is<SimpleFlatMacroEnum::First>());
    CHECK(get_maybe_simple_dict(0).is<MaybeSimpleDict::Yeah>());
    CHECK_EQ(get_maybe_simple_dict(0).get_if<MaybeSimpleDict::Yeah>()->d.text, "");
    CHECK(get_maybe_simple_dict(1).is<MaybeSimpleDict::Nah>());
    CHECK(get_maybe_object(0).is<MaybeObject::Obj>());
    CHECK(get_maybe_object(0).get_if<MaybeObject::Obj>()->p->get_color() == Color::Red);
    CHECK(get_maybe_object(1).is<MaybeObject::Nah>());
}

static void arcs_and_objects() {
    {
        auto coveralls = Coveralls::new_("test_arcs");
        // One reference in the handle, one for the `Arc<Self>` receiver.
        CHECK_EQ(coveralls->strong_count(), 2u);
        CHECK(!coveralls->get_other().has_value());
        coveralls->take_other(coveralls);
        CHECK_EQ(coveralls->strong_count(), 3u);
        CHECK_EQ(get_num_alive(), 1u);
        CHECK_EQ((*coveralls->get_other())->get_name(), "test_arcs");
        CHECK_THROWS(coveralls->take_other_fallible(), CoverallError_TooManyHoles);
        CHECK_THROWS(coveralls->take_other_panic("expected panic: with an arc!"), uniffi::PanicError);
        coveralls->take_other(std::nullopt);
        CHECK_EQ(coveralls->strong_count(), 2u);
    }
    CHECK_EQ(get_num_alive(), 0u);

    {
        auto coveralls = Coveralls::new_("test_return_objects");
        auto c2 = coveralls->clone_me();
        CHECK_EQ(c2->get_name(), coveralls->get_name());
        CHECK_EQ(get_num_alive(), 2u);
        CHECK_EQ(c2->strong_count(), 2u);
        coveralls->take_other(c2);
        CHECK_EQ(coveralls->strong_count(), 2u);
        CHECK_EQ(c2->strong_count(), 3u);
        c2.reset();
        CHECK_EQ(get_num_alive(), 2u);
        coveralls->add_patch(Patch::new_(Color::Red));
        coveralls->add_repair(Repair{std::chrono::system_clock::now(), Patch::new_(Color::Blue)});
        CHECK_EQ(coveralls->get_repairs().size(), 2u);
    }
    CHECK_EQ(get_num_alive(), 0u);

    CHECK_THROWS(FalliblePatch::new_(), CoverallError_TooManyHoles);
    CHECK_THROWS(FalliblePatch::secondary(), CoverallError_TooManyHoles);

    DictWithDefaults d;
    CHECK_EQ(d.name, "default-value");
    CHECK(!d.category.has_value());
    CHECK_EQ(d.integer, 31u);
    CHECK(d.item_list.empty() && d.item_map.empty());

    auto dicts = Coveralls::new_("test_dict");
    CHECK_EQ(dicts->get_dict("answer", 42)["answer"], 42u);
    CHECK_EQ(dicts->get_dict2("answer", 42)["answer"], 42u);
    CHECK_EQ(dicts->get_dict3(31, 42)[31], 42u);
    CHECK(dicts->reverse({'1', '2', '3'}) == (std::vector<uint8_t>{'3', '2', '1'}));
    CHECK(dicts->set_and_get_empty_struct(EmptyStruct{}) == EmptyStruct{});

    // A record that Rust can only return: passing one in is an error, not a crash.
    CHECK_THROWS(try_input_return_only_dict(ReturnOnlyDict{std::make_shared<CoverallFlatError_TooManyVariants>("x")}),
                 uniffi::Exception);
}

static void traits() {
    test_getters(std::make_shared<CppGetters>());

    auto nodes = get_traits();
    CHECK_EQ(nodes[0]->name(), "node-1");
    CHECK_EQ(nodes[0]->strong_count(), 2u);
    CHECK_EQ(nodes[1]->name(), "node-2");
    CHECK_EQ(nodes[1]->strong_count(), 2u);
    nodes[0]->set_parent(nodes[1]);
    CHECK_EQ(nodes[1]->strong_count(), 3u);
    CHECK(ancestor_names(nodes[0]) == std::vector<std::string>{"node-2"});
    CHECK(ancestor_names(nodes[1]).empty());
    CHECK_EQ((*nodes[0]->get_parent())->name(), "node-2");

    auto cpp_node = std::make_shared<CppNode>();
    nodes[1]->set_parent(std::shared_ptr<NodeTrait>(cpp_node));
    CHECK(ancestor_names(nodes[0]) == (std::vector<std::string>{"node-2", "node-cpp"}));
    CHECK(ancestor_names(nodes[1]) == std::vector<std::string>{"node-cpp"});
    CHECK(ancestor_names(cpp_node).empty());

    nodes[1]->set_parent(std::nullopt);
    cpp_node->set_parent(nodes[0]);
    CHECK(ancestor_names(cpp_node) == (std::vector<std::string>{"node-1", "node-2"}));
    CHECK(ancestor_names(nodes[0]) == std::vector<std::string>{"node-2"});
    cpp_node->set_parent(std::nullopt);
    nodes[0]->set_parent(std::nullopt);

    // A struct that is also a trait.
    auto node = Node::new_("node");
    CHECK(starts_with(node->debug_string(), "Node { name: Some(\"node\"), parent: Mutex { "));
    CHECK(node->get_parent().has_value());
    CHECK(starts_with(node->describe_parent(), "Some(Node { name: Some(\"via node\"), parent: Mutex { "));
    node->set_parent(std::shared_ptr<NodeTrait>(std::make_shared<CppNode>()));
    CHECK(starts_with(node->describe_parent(), "Some(UniFFICallbackHandlerNodeTrait { handle: "));

    // Round trips must not free what the other side still uses.
    test_round_trip_through_rust(make_rust_getters());
    test_round_trip_through_foreign(std::make_shared<CppGetters>());

    auto utils = get_string_util_traits();
    CHECK_EQ(utils[0]->concat("cow", "boy"), "cowboy");
    CHECK_EQ(utils[1]->concat("cow", "boy"), "cowboy");
    CHECK(async_bool(true).get());
}

int main() {
    some_dict();
    none_dict();
    constructors_and_errors();
    enums();
    arcs_and_objects();
    traits();
    CHECK_EQ(get_num_alive(), 0u);
    return check::report();
}
