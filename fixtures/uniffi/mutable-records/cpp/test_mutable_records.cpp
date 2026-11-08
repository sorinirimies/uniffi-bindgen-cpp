// Port of uniffi-rs v0.32.2 fixtures/mutable-records/tests/bindings/test_mutable_records.kts
// C++ records are plain structs: every record is mutable, whatever uniffi.toml says for Kotlin/Swift.
#include <string>

#include "check.hpp"
#include "mutable_records.hpp"

using namespace mutable_records;

int main() {
    MutableRecord mutable_record{"initial"};
    mutable_record.value = "modified";
    CHECK_EQ(mutable_record.value, std::string("modified"));

    ImmutableRecord immutable{"fixed"};
    CHECK_EQ(immutable.value, std::string("fixed"));
    return check::report();
}
