// Port of uniffi-rs v0.32.2 fixtures/enum-types/tests/bindings/test_enum_types.py
#include "check.hpp"
#include "enum_types.hpp"

using namespace enum_types;

int main() {
    // Discriminants, including explicit and non-int32 ones.
    CHECK_EQ(static_cast<int32_t>(Animal::Dog), 0);
    CHECK_EQ(static_cast<int32_t>(Animal::Cat), 1);
    CHECK(get_animal(std::nullopt) == Animal::Dog);
    CHECK_EQ(static_cast<int32_t>(AnimalNoReprInt::Dog), 3);
    CHECK_EQ(static_cast<int32_t>(AnimalNoReprInt::Cat), 4);
    CHECK_EQ(static_cast<uint8_t>(AnimalUInt::Dog), 3);
    CHECK_EQ(static_cast<uint8_t>(AnimalUInt::Cat), 4);
    CHECK_EQ(static_cast<uint64_t>(AnimalLargeUInt::Dog), 4294967295ULL + 3);
    CHECK_EQ(static_cast<uint64_t>(AnimalLargeUInt::Cat), 4294967295ULL + 4);
    CHECK_EQ(static_cast<int8_t>(AnimalSignedInt::Dog), -3);
    CHECK_EQ(static_cast<int8_t>(AnimalSignedInt::Cat), -2);
    CHECK_EQ(static_cast<int8_t>(AnimalSignedInt::Koala), -1);
    CHECK_EQ(static_cast<int8_t>(AnimalSignedInt::Wallaby), 0);
    CHECK_EQ(static_cast<int8_t>(AnimalSignedInt::Wombat), 1);

    // Containers: variants holding objects and records.
    auto dog = get_animal_enum(Animal::Dog);
    CHECK(dog.is<AnimalEnum::Dog>());
    CHECK_EQ(dog.get_if<AnimalEnum::Dog>()->v0->get_record().name, "dog");
    // Objects with an exported Eq compare by value inside an enum.
    CHECK(get_animal_enum(Animal::Dog) == get_animal_enum(Animal::Dog));
    auto cat = get_animal_enum(Animal::Cat);
    CHECK_EQ(cat.get_if<AnimalEnum::Cat>()->v0.name, "cat");
    CHECK(get_animal_enum(Animal::Cat) == get_animal_enum(Animal::Cat));
    CHECK(get_animal_enum(Animal::Dog) != get_animal_enum(Animal::Cat));

    // Variant fields with default values.
    NamedEnumWithDefaults::I i;
    CHECK_EQ(i.d, 0);
    CHECK_EQ(i.e, 1);
    NamedEnumWithDefaults e{NamedEnumWithDefaults::I{0, 2}};
    CHECK_EQ(e.get_if<NamedEnumWithDefaults::I>()->e, 2);

    // Boxed variants and records (uniffi 0.32).
    auto boxed = create_boxed_enum("hello");
    CHECK(boxed.is<EnumWithBoxedVariant::Boxed>());
    CHECK_EQ(get_boxed_enum_value(boxed), "hello");
    EnumWithBoxedVariant empty{EnumWithBoxedVariant::Empty{}};
    CHECK_EQ(get_boxed_enum_value(empty), "empty");
    CHECK_EQ(roundtrip_boxed_record(BoxedContent{"world"})->value, "world");
    return check::report();
}
