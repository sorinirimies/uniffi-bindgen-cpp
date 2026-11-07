// Port of uniffi-rs v0.31.2 fixtures/rename/tests/test_rename.py
// The [bindings.cpp.rename] section of uniffi.toml renames the "Binding" items.
#include "check.hpp"
#include "uniffi_fixture_rename.hpp"

using namespace uniffi_fixture_rename;

int main() {
    // Renamed in Rust.
    RenamedRecord record{42};
    CHECK_EQ(record.item, 42);
    RenamedEnum variant{RenamedEnum::RenamedVariant{}};
    RenamedEnum with_record{RenamedEnum::Record{record}};
    CHECK(variant.is<RenamedEnum::RenamedVariant>());
    CHECK_EQ(with_record.get_if<RenamedEnum::Record>()->v0.item, 42);
    CHECK_EQ(renamed_function(record).get_if<RenamedEnum::Record>()->v0.item, 42);
    auto obj = RenamedObject::renamed_constructor(123);
    CHECK_EQ(obj->renamed_method(), 123);
    RenamedError_RenamedErrorVariant error;
    CHECK(dynamic_cast<RenamedError*>(&error) != nullptr);
    CHECK_EQ(create_trait_impl(5)->renamed_trait_method(10), 50);

    // Renamed for C++ by uniffi.toml.
    CppRecord cpp_record{42};
    CHECK_EQ(cpp_record.cpp_item, 42);
    CppEnum cpp_enum{CppEnum::CppRecord{cpp_record}};
    CHECK_EQ(cpp_enum.get_if<CppEnum::CppRecord>()->v0.cpp_item, 42);
    CppEnumWithFields fields{CppEnumWithFields::CppVariantA{1}};
    CHECK_EQ(fields.get_if<CppEnumWithFields::CppVariantA>()->cpp_int, 1u);
    CppEnumWithFields both{CppEnumWithFields::CppRecord{cpp_record, 1}};
    CHECK(both.get_if<CppEnumWithFields::CppRecord>()->cpp_record == cpp_record);
    CHECK_EQ(cpp_function(cpp_record).get_if<CppEnum::CppRecord>()->v0.cpp_item, 42);
    auto cpp_object = CppObject::new_(5);
    CHECK_EQ(cpp_object->cpp_method(10), 15);
    CppError_CppSimple cpp_error;
    CHECK(dynamic_cast<CppError*>(&cpp_error) != nullptr);
    CHECK_EQ(create_binding_trait_impl(5)->cpp_trait_method(10), 50);
    return check::report();
}
