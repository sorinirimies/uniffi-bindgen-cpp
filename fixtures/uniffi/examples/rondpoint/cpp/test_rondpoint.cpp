// Port of uniffi-rs v0.32.2 examples/rondpoint/tests/bindings/test_rondpoint.py
#include <cfloat>
#include <cstdint>
#include <limits>
#include <sstream>
#include <vector>

#include "check.hpp"
#include "rondpoint.hpp"

using namespace rondpoint;

template <class T, class F>
void round_trips(const std::vector<T>& values, F identity) {
    for (const auto& v : values) {
        CHECK(identity(v) == v);
    }
}

template <class T, class F>
void stringifies(const std::vector<T>& values, F to_string) {
    for (const auto& v : values) {
        std::ostringstream expected;
        if constexpr (std::is_same_v<T, bool>) {
            expected << (v ? "true" : "false");
        } else if constexpr (sizeof(T) == 1) {
            expected << static_cast<int>(v);
        } else {
            expected << v;
        }
        CHECK_EQ(to_string(v), expected.str());
    }
}

int main() {
    Dictionnaire dico{Enumeration::Deux, true, 0, 123456789};
    CHECK(copie_dictionnaire(dico) == dico);
    CHECK(copie_enumeration(Enumeration::Deux) == Enumeration::Deux);
    CHECK(copie_enumerations({Enumeration::Un, Enumeration::Deux}) ==
          (std::vector<Enumeration>{Enumeration::Un, Enumeration::Deux}));

    using E = EnumerationAvecDonnees;
    std::map<std::string, E> carte{
        {"0", E{E::Zero{}}},
        {"1", E{E::Un{1}}},
        {"2", E{E::Deux{2, "deux"}}},
    };
    CHECK(copie_carte(carte) == carte);
    CHECK(switcheroo(false));
    CHECK(E{E::Zero{}} != E{E::Un{1}});
    CHECK(E{E::Un{1}} == E{E::Un{1}});
    CHECK(E{E::Un{1}} != E{E::Un{2}});

    auto rt = Retourneur::new_();
    auto call = [&](auto method) { return [&, method](auto v) { return ((*rt).*method)(v); }; };
    round_trips<bool>({true, false}, call(&Retourneur::identique_boolean));
    round_trips<int8_t>({INT8_MIN, -1, 0, 1, INT8_MAX}, call(&Retourneur::identique_i8));
    round_trips<uint8_t>({0x00, 0x12, 0xFF}, call(&Retourneur::identique_u8));
    round_trips<int16_t>({INT16_MIN, -1, 0, 1, INT16_MAX}, call(&Retourneur::identique_i16));
    round_trips<uint16_t>({0x0000, 0x1234, 0xFFFF}, call(&Retourneur::identique_u16));
    round_trips<int32_t>({INT32_MIN, -1, 0, 1, INT32_MAX}, call(&Retourneur::identique_i32));
    round_trips<uint32_t>({0x00000000, 0x12345678, 0xFFFFFFFF}, call(&Retourneur::identique_u32));
    round_trips<int64_t>({INT64_MIN, -1, 0, 1, INT64_MAX}, call(&Retourneur::identique_i64));
    round_trips<uint64_t>({0, 0x1234567890ABCDEFULL, UINT64_MAX}, call(&Retourneur::identique_u64));
    round_trips<float>({0.0f, 0.5f, 0.25f, 1.0f, 1.0f / 3}, call(&Retourneur::identique_float));
    round_trips<double>({0.0, 0.5, 0.25, 1.0, 1.0 / 3, DBL_MAX, DBL_MIN},
                        call(&Retourneur::identique_double));
    round_trips<std::string>(
        {"", "abc", "\xC3\xA9t\xC3\xA9",
         "\xDA\x9A\xD9\x8A \xD9\x84\xD8\xA7\xD8\xB3 \xD8\xAA\xD9\x87 \xD9\x84\xD9\x88\xD8\xB3\xD8\xAA\xD9\x84\xD9\x88",
         "\xF0\x9F\x98\xBB" "emoji \xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA7\xE2\x80\x8D\xF0\x9F\x91\xA6multi-emoji"},
        [&](const std::string& v) { return rt->identique_string(v); });

    auto st = Stringifier::new_();
    CHECK_EQ(st->well_known_string("c++"), "uniffi \xF0\x9F\x92\x9A c++!");
    auto str = [&](auto method) { return [&, method](auto v) { return ((*st).*method)(v); }; };
    stringifies<bool>({true, false}, str(&Stringifier::to_string_boolean));
    stringifies<int8_t>({INT8_MIN, -1, 0, 1, INT8_MAX}, str(&Stringifier::to_string_i8));
    stringifies<uint8_t>({0x00, 0x12, 0xFF}, str(&Stringifier::to_string_u8));
    stringifies<int16_t>({INT16_MIN, -1, 0, 1, INT16_MAX}, str(&Stringifier::to_string_i16));
    stringifies<uint16_t>({0x0000, 0x1234, 0xFFFF}, str(&Stringifier::to_string_u16));
    stringifies<int32_t>({INT32_MIN, -1, 0, 1, INT32_MAX}, str(&Stringifier::to_string_i32));
    stringifies<uint32_t>({0x00000000, 0x12345678, 0xFFFFFFFF}, str(&Stringifier::to_string_u32));
    stringifies<int64_t>({INT64_MIN, -1, 0, 1, INT64_MAX}, str(&Stringifier::to_string_i64));
    stringifies<uint64_t>({0, 0x1234567890ABCDEFULL, UINT64_MAX}, str(&Stringifier::to_string_u64));
    CHECK_EQ(st->to_string_float(0.5f), "0.5");
    CHECK_EQ(st->to_string_float(1.0f), "1");
    CHECK_EQ(st->to_string_float(1.0f / 3), "0.33333334");
    CHECK_EQ(st->to_string_double(0.25), "0.25");
    CHECK_EQ(st->to_string_double(0.0), "0");

    // Default arguments and default fields.
    auto op = Optionneur::new_();
    CHECK_EQ(op->sinon_string(), "default");
    CHECK_EQ(op->sinon_boolean(), false);
    CHECK(op->sinon_sequence().empty());
    CHECK(!op->sinon_null().has_value());
    CHECK(op->sinon_zero() == std::optional<int32_t>(0));
    CHECK_EQ(op->sinon_u8_dec(), 42);
    CHECK_EQ(op->sinon_i8_dec(), -42);
    CHECK_EQ(op->sinon_u16_hex(), 0xffff);
    CHECK_EQ(op->sinon_i64_hex(), INT64_MAX);
    CHECK_EQ(op->sinon_u32_oct(), 0755u);
    CHECK_EQ(op->sinon_f32(), 42.0f);
    CHECK_EQ(op->sinon_f64(), 42.1);
    CHECK(op->sinon_enum() == Enumeration::Trois);
    CHECK_EQ(op->sinon_string("foo"), "foo");

    OptionneurDictionnaire defaults;
    CHECK(rt->identique_optionneur_dictionnaire(defaults) == defaults);
    CHECK(defaults.enumeration_var == Enumeration::Deux);
    CHECK_EQ(defaults.string_var, "default");
    return check::report();
}
