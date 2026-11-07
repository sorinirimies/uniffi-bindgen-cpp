// Port of uniffi-rs v0.31.2 fixtures/struct-default-values/tests/bindings/test_struct_default_values.py
#include "check.hpp"
#include "struct_default_values.hpp"

using namespace struct_default_values;

int main() {
    const std::string url = "https://mozilla.github.io/uniffi-rs";

    // Only the fields without a default value set.
    Bookmark bookmark;
    bookmark.position = 2;
    bookmark.url = url;
    CHECK(!bookmark.guid.has_value());
    CHECK_EQ(bookmark.position, 2);
    CHECK_EQ(bookmark.url, url);
    CHECK(!bookmark.last_modified.has_value());

    // All fields, in order.
    Bookmark other{"c0ffee", 3, std::nullopt, url, std::nullopt};
    CHECK(other.guid == std::optional<std::string>("c0ffee"));
    CHECK_EQ(other.position, 3);
    CHECK_EQ(other.url, url);
    return check::report();
}
