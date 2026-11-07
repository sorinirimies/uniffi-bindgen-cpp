// Port of uniffi-rs v0.32.2 fixtures/ext-types/lib/tests/bindings/test_imported_types.py
// One library, five components; each has its own header and namespace.
#include "check.hpp"
#include "ext_types_custom.hpp"
#include "imported_types_lib.hpp"
#include "imported_types_sublib.hpp"
#include "uniffi_one_ns.hpp"

using namespace imported_types_lib;
using uniffi_one_ns::UniffiOneEnum;
using uniffi_one_ns::UniffiOneType;

class CppUniffiOneImpl : public uniffi_one_ns::UniffiOneTrait {
public:
    std::string hello() override { return "Hello from C++"; }
};

int main() {
    // A trait from another crate, implemented in C++ (uniffi-rs #2343).
    CHECK_EQ(invoke_uniffi_one_trait(std::make_shared<CppUniffiOneImpl>()), "Hello from C++");

    auto ct = get_combined_type(std::nullopt);
    CHECK_EQ(ct.uot.sval, "hello");
    CHECK_EQ(ct.guid, "a-guid");
    CHECK_EQ(ct.url, "http://example.com/");
    CHECK(get_combined_type(ct) == ct);
    CHECK_EQ(ct.ecd.sval, "ecd");
    CHECK_EQ(get_external_crate_interface("foo")->value(), "foo");

    auto t = imported_types_sublib::get_trait_impl();
    CHECK_EQ(t->hello(), "sub-lib trait impl says hello");
    imported_types_sublib::SubLibType sub{std::nullopt, t, std::nullopt};
    CHECK(imported_types_sublib::get_sub_type(sub).maybe_trait.has_value());
    ObjectsType ot{t, std::nullopt, sub};
    CHECK(ot.maybe_trait.has_value());
    CHECK(!ot.maybe_interface.has_value());
    CHECK(!get_uniffi_one_trait(std::nullopt).has_value());

    const std::string url = "http://example.com/";
    CHECK_EQ(get_url(url), url);
    CHECK(get_urls({url}) == std::vector<std::string>{url});
    CHECK(get_maybe_url(url) == std::optional<std::string>(url));
    CHECK(!get_maybe_url(std::nullopt).has_value());
    CHECK(get_maybe_urls({url, std::nullopt}) == (std::vector<std::optional<std::string>>{url, std::nullopt}));

    CHECK_EQ(ext_types_custom::get_guid("guid"), "guid");
    CHECK_EQ(ext_types_custom::get_ouid("ouid"), "ouid");
    CHECK_EQ(ext_types_custom::get_nested_guid("uuid"), "uuid");
    CHECK_EQ(get_imported_handle_u8(std::nullopt), 3);

    UniffiOneType t1{"hello"};
    CHECK(get_uniffi_one_type(t1) == t1);
    CHECK(get_maybe_uniffi_one_type(t1) == std::optional<UniffiOneType>(t1));
    CHECK(!get_maybe_uniffi_one_type(std::nullopt).has_value());
    CHECK(get_uniffi_one_types({t1}) == std::vector<UniffiOneType>{t1});
    CHECK(get_maybe_uniffi_one_types({t1, std::nullopt}) ==
          (std::vector<std::optional<UniffiOneType>>{t1, std::nullopt}));

    auto e = UniffiOneEnum::One;
    CHECK(get_uniffi_one_enum(e) == e);
    CHECK(get_maybe_uniffi_one_enum(e) == std::optional<UniffiOneEnum>(e));
    CHECK(!get_maybe_uniffi_one_enum(std::nullopt).has_value());
    CHECK(get_uniffi_one_enums({e}) == std::vector<UniffiOneEnum>{e});
    CHECK(get_maybe_uniffi_one_enums({e, std::nullopt}) ==
          (std::vector<std::optional<UniffiOneEnum>>{e, std::nullopt}));

    uniffi_one_ns::UniffiOneProcMacroType pm{"hello"};
    CHECK(get_uniffi_one_proc_macro_type(pm) == pm);
    CHECK(uniffi_one_ns::get_my_proc_macro_type(pm) == pm);

    CHECK_EQ(get_imported_guid("guid"), "guid");
    CHECK_EQ(get_imported_ouid("ouid"), "ouid");
    CHECK_EQ(get_imported_nested_guid(std::nullopt), "nested");
    CHECK_EQ(get_imported_nested_ouid(std::nullopt), "nested");
    CHECK_EQ(get_nested_external_guid(std::nullopt), "nested-external");
    CHECK_EQ(get_nested_external_ouid(std::nullopt), "nested-external-ouid");

    // Errors defined in another crate.
    CHECK_THROWS(throw_uniffi_one_error(), uniffi_one_ns::UniffiOneError);
    CHECK_THROWS(throw_uniffi_one_error_async().get(), uniffi_one_ns::UniffiOneError);
    CHECK_THROWS(throw_uniffi_one_error_interface(), uniffi_one_ns::UniffiOneErrorInterface);
    CHECK(uniffi_one_ns::get_uniffi_one_async().get() == UniffiOneEnum::One);
    return check::report();
}
