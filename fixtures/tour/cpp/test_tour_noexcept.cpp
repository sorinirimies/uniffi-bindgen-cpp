// End-to-end test of bindings generated with `--no-exceptions`, built with
// -fno-exceptions: Rust errors come back in a `uniffi::Result`, C++ callbacks
// return one, and what can only be a bug reaches the fatal handler.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

#include "tour.hpp"

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++checks;                                                                \
        if (!(cond)) {                                                           \
            ++failures;                                                          \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                        \
    } while (0)

#if UNIFFI_CPP_EXCEPTIONS
#error "this test must be built with -fno-exceptions"
#endif

using namespace std::chrono_literals;

static void test_errors() {
    CHECK(tour::fail_data(0).has_value());

    auto bad_code = tour::fail_data(1);
    CHECK(!bad_code);
    CHECK(bad_code.error()->uniffi_variant_index() == 1);
    auto as_bad_code = std::dynamic_pointer_cast<tour::DataError_BadCode>(bad_code.error());
    CHECK(as_bad_code && as_bad_code->code == 42);

    auto too_long = tour::fail_data(2);
    CHECK(!too_long && too_long.error()->uniffi_variant_index() == 2);
    CHECK(std::dynamic_pointer_cast<tour::DataError_TooLong>(too_long.error())->v0 == 9);

    CHECK(tour::fail_flat(0).value() == 7);
    CHECK(tour::fail_flat(1).value_or(0) == 0);
    auto denied = tour::fail_flat(2);
    CHECK(std::dynamic_pointer_cast<tour::FlatError_Denied>(denied.error())->v0 == "nope");

    auto io = tour::fail_message(1);
    CHECK(!io && std::dynamic_pointer_cast<tour::MessageError_Io>(io.error())->message == "io failure: disk");
    CHECK(std::string(io.error()->what()).find("disk") != std::string::npos);
}

static void test_objects() {
    auto refused = tour::Counter::checked(-1);
    CHECK(!refused && std::dynamic_pointer_cast<tour::FlatError_Denied>(refused.error()));
    auto counter = tour::Counter::checked(3).value();
    CHECK(counter->get() == 3);
    CHECK(counter->try_sub(1).value() == 2);
    auto too_much = counter->try_sub(1000);
    CHECK(!too_much && std::dynamic_pointer_cast<tour::DataError_BadCode>(too_much.error())->code == 1000);
}

class RecordingListener : public tour::Listener {
public:
    int32_t on_event(const std::string& name, int32_t value) override {
        (void)name;
        return value * 10;
    }
    void on_done() override { done = true; }
    uniffi::Result<std::string, std::shared_ptr<tour::FlatError>> fallible(int32_t n) override {
        if (n < 0) {
            return uniffi::Err{std::make_shared<tour::FlatError_Denied>("negative")};
        }
        return uniffi::Ok{"ok" + std::to_string(n)};
    }
    bool done = false;
};

static void test_callbacks() {
    auto listener = std::make_shared<RecordingListener>();
    CHECK(tour::notify(listener, 4) == 60);
    CHECK(listener->done);
    CHECK(tour::try_listener(listener, 5).value() == "ok5");
    auto denied = tour::try_listener(listener, -1);
    CHECK(!denied && std::dynamic_pointer_cast<tour::FlatError_Denied>(denied.error())->v0 == "negative");
}

class CppGreeter : public tour::Greeter {
public:
    std::string greet(const std::string& name) override { return "c++ says hi to " + name; }
};

static void test_trait_objects() {
    CHECK(tour::use_greeter(tour::rust_greeter(), "y") == "rust says hi to y");
    CHECK(tour::use_greeter(std::make_shared<CppGreeter>(), "z") == "c++ says hi to z");
}

// Without exceptions there is no Future::from(std::future): complete a Promise.
class CppSource : public tour::AsyncSource {
public:
    uniffi::Future<std::string> fetch(const std::string& key) override {
        uniffi::Promise<std::string> promise;
        std::thread([promise, key] {
            std::this_thread::sleep_for(20ms);
            promise.set_value("fetched " + key);
        }).detach();
        return promise.get_future();
    }
};

static void test_async() {
    CHECK(tour::slow_add(2, 3).get() == 5);
    CHECK(tour::slow_fail(false).get().value() == "fine");
    auto failed = tour::slow_fail(true).get();
    CHECK(!failed && failed.error()->uniffi_variant_index() == 3);
    tour::slow_unit().get();
    CHECK(tour::Worker::new_("w")->work(3).get() == "w did 3");
    CHECK(tour::fetch_via(std::make_shared<CppSource>(), "k").get() == "fetched k");

    // Cancelled: no exception to throw, so the future says so instead.
    auto cancelled = tour::slow_add(1, 2);
    cancelled.cancel();
    cancelled.wait();
    CHECK(cancelled.is_cancelled());
    auto finished = tour::slow_add(1, 2);
    finished.wait();
    CHECK(!finished.is_cancelled() && finished.get() == 3);
}

// A Rust panic can only be a bug: it goes to the fatal handler, which must not
// return. This one checks the message and ends the test.
static void on_fatal(const char* message) {
    CHECK(std::strstr(message, "boom") != nullptr);
    std::printf("%d checks, %d failures\n", checks, failures);
    std::fflush(stdout);
    std::_Exit(failures == 0 ? 0 : 1);
}

int main() {
    test_errors();
    test_objects();
    test_callbacks();
    test_trait_objects();
    test_async();
    uniffi::set_fatal_handler(&on_fatal);
    tour::panics();
    std::fprintf(stderr, "the fatal handler did not end the test\n");
    return 1;
}
