// Port of uniffi-rs v0.32.2 fixtures/ext-types/custom-types/tests/bindings/test_custom_types.py
#include "check.hpp"
#include "ext_types_custom.hpp"

using namespace ext_types_custom;

class TestCallback : public GuidCallback {
public:
    Guid run(const Guid& guid) override {
        saw_guid = guid;
        return guid;
    }
    Guid saw_guid;
};

static bool contains(const char* text, const std::string& part) {
    return std::string(text).find(part) != std::string::npos;
}

int main() {
    CHECK_EQ(get_guid(std::nullopt), "NewGuid");
    CHECK_EQ(get_guid("SomeGuid"), "SomeGuid");
    CHECK_EQ(get_ouid(std::nullopt), "Ouid");

    auto helper = get_guid_helper(std::nullopt);
    CHECK_EQ(helper.guid, "first-guid");
    CHECK(helper.guids == (std::vector<Guid>{"second-guid", "third-guid"}));
    CHECK(!helper.maybe_guid.has_value());

    // Lifting a custom type can fail in Rust: a panic, or an error the function declares.
    CHECK_THROWS_AS(get_guid(""), uniffi::Exception, CHECK(contains(e.what(), "The Guid is too short")));
    CHECK_THROWS_AS(get_guid("unexpected"), uniffi::Exception,
                    CHECK(contains(e.what(), "Something unexpected went wrong")));
    CHECK_THROWS_AS(get_guid("panic"), uniffi::PanicError, CHECK(contains(e.what(), "guid value caused a panic!")));
    CHECK_THROWS(try_get_guid(""), GuidError_TooShort);
    CHECK_THROWS(try_get_guid("unexpected"), uniffi::Exception);
    CHECK_THROWS(try_get_guid("panic"), uniffi::PanicError);

    auto callback = std::make_shared<TestCallback>();
    CHECK_EQ(run_callback(callback), "callback-test-payload");
    CHECK_EQ(callback->saw_guid, "callback-test-payload");

    CHECK_EQ(get_handle_u8(std::nullopt), 2);
    get_nested_object(InnerObject::new_());
    return check::report();
}
