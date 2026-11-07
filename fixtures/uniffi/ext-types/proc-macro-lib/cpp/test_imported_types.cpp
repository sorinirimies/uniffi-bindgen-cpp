// Port of uniffi-rs v0.31.2 fixtures/ext-types/proc-macro-lib/tests/bindings/test_imported_types.py
#include "check.hpp"
#include "ext_types_custom.hpp"
#include "imported_types_lib.hpp"
#include "uniffi_one_ns.hpp"

using namespace imported_types_lib;
using uniffi_one_ns::UniffiOneEnum;
using uniffi_one_ns::UniffiOneType;

int main() {
    auto ct = get_combined_type(std::nullopt);
    CHECK_EQ(ct.uot.sval, "hello");
    CHECK_EQ(ct.guid, "a-guid");
    CHECK_EQ(ct.url, "http://example.com/");
    CHECK(get_combined_type(ct) == ct);
    auto ot = get_objects_type(std::nullopt);
    CHECK(!ot.maybe_trait.has_value());
    CHECK(!ot.maybe_interface.has_value());
    CHECK(!get_uniffi_one_trait(std::nullopt).has_value());

    const std::string url = "http://example.com/";
    CHECK_EQ(get_url(url), url);
    CHECK(get_urls({url}) == std::vector<std::string>{url});
    CHECK(get_maybe_url(url) == std::optional<std::string>(url));
    CHECK(!get_maybe_url(std::nullopt).has_value());

    UniffiOneType t1{"hello"};
    CHECK(get_uniffi_one_type(t1) == t1);
    CHECK(get_maybe_uniffi_one_type(t1) == std::optional<UniffiOneType>(t1));
    CHECK(get_uniffi_one_types({t1}) == std::vector<UniffiOneType>{t1});
    CHECK(uniffi_one_ns::get_uniffi_one_async().get() == UniffiOneEnum::One);
    CHECK(get_uniffi_one_type_async(t1).get() == t1);
    uniffi_one_ns::UniffiOneProcMacroType pm{"hello"};
    CHECK(get_uniffi_one_proc_macro_type(pm) == pm);

    auto e = UniffiOneEnum::One;
    CHECK(get_uniffi_one_enum(e) == e);
    CHECK(get_maybe_uniffi_one_enums({e, std::nullopt}) ==
          (std::vector<std::optional<UniffiOneEnum>>{e, std::nullopt}));

    auto g = get_guid_procmacro(std::nullopt);
    CHECK(get_guid_procmacro(g) == g);
    auto guid = ext_types_custom::get_guid(std::nullopt);
    CHECK(ext_types_custom::get_guid(guid) == guid);
    auto uuid = get_uuid(std::nullopt);
    CHECK(get_uuid(uuid) == uuid);
    CHECK_EQ(get_uuid_value(uuid), "new");
    auto handle = get_newtype_handle(std::nullopt);
    CHECK(get_newtype_handle(handle) == handle);
    CHECK_EQ(get_newtype_handle_value(handle), 42);
    CHECK_EQ(get_ouid2(), "hello");
    return check::report();
}
