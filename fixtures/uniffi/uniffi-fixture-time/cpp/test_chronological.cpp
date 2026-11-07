// Port of uniffi-rs v0.31.2 fixtures/uniffi-fixture-time/tests/bindings/test_chronological.py
#include "check.hpp"
#include "chronological.hpp"

using namespace chronological;
using namespace std::chrono;

static uniffi::Timestamp at(int64_t seconds, int64_t micros = 0) {
    return uniffi::Timestamp(std::chrono::seconds(seconds) + std::chrono::microseconds(micros));
}

int main() {
    auto one_s_one_us = duration_cast<uniffi::Duration>(seconds(1) + microseconds(1));
    CHECK(add(at(100, 1), one_s_one_us) == at(101, 2));
    CHECK(diff(at(101, 2), at(100, 1)) == one_s_one_us);
    // Before the Unix epoch: 1955-11-05T00:06:00.283001Z.
    auto before_epoch = at(-446168040, 283001);
    CHECK(add(before_epoch, one_s_one_us) == at(-446168039, 283002));
    CHECK_THROWS(diff(at(100), at(101)), ChronologicalError_TimeDiffError);

    auto before = system_clock::now();
    auto rust_now = now();
    auto after = system_clock::now();
    CHECK(before <= rust_now && rust_now <= after);

    CHECK(optional(now(), uniffi::Duration(0)));
    CHECK(!optional(std::nullopt, uniffi::Duration(0)));
    CHECK(!optional(now(), std::nullopt));

    CHECK(return_timestamp(before_epoch) == before_epoch);
    CHECK(return_duration(one_s_one_us) == one_s_one_us);
    CHECK_EQ(get_seconds_before_unix_epoch(set_seconds_before_unix_epoch(100)), 100u);
    CHECK(get_pre_epoch_timestamp() < at(0));
    return check::report();
}
