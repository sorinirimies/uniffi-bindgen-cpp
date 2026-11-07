// Port of uniffi-rs v0.31.2 fixtures/futures/tests/bindings/test_futures.py
#include <atomic>
#include <chrono>
#include <thread>

#include "check.hpp"
#include "futures.hpp"

using namespace futures;
using namespace std::chrono;
using namespace std::chrono_literals;

static double seconds_since(steady_clock::time_point t0) {
    return duration<double>(steady_clock::now() - t0).count();
}

/// Completes `promise` after `ms` on a timer thread, unless it is cancelled first.
template <class T, class F>
void after(int32_t ms, uniffi::Promise<T> promise, F complete) {
    std::thread([ms, promise, complete]() mutable {
        std::this_thread::sleep_for(milliseconds(ms));
        if (!promise.is_cancelled()) {
            complete(promise);
        }
    }).detach();
}

class CppAsyncParser : public AsyncParser {
public:
    std::atomic<int> completed_delays{0};

    uniffi::Future<std::string> as_string(int32_t delay_ms, int32_t value) override {
        uniffi::Promise<std::string> promise;
        after(delay_ms, promise, [value](auto& p) { p.set_value(std::to_string(value)); });
        return promise.get_future();
    }

    uniffi::Future<int32_t> try_from_string(int32_t delay_ms, const std::string& value) override {
        uniffi::Promise<int32_t> promise;
        after(delay_ms, promise, [value](auto& p) {
            if (value == "force-unexpected-exception") {
                p.set_exception(std::make_exception_ptr(std::runtime_error("UnexpectedException")));
                return;
            }
            try {
                size_t used = 0;
                int parsed = std::stoi(value, &used);
                if (used != value.size()) {
                    throw std::invalid_argument(value);
                }
                p.set_value(parsed);
            } catch (const std::exception&) {
                p.set_exception(std::make_exception_ptr(ParserError_NotAnInt()));
            }
        });
        return promise.get_future();
    }

    uniffi::Future<void> delay(int32_t delay_ms) override {
        uniffi::Promise<void> promise;
        after(delay_ms, promise, [this](auto& p) {
            ++completed_delays;
            p.set_value();
        });
        return promise.get_future();
    }

    uniffi::Future<void> try_delay(const std::string& delay_ms) override {
        int ms = 0;
        try {
            ms = std::stoi(delay_ms);
        } catch (const std::exception&) {
            return uniffi::Future<void>::failed(std::make_exception_ptr(ParserError_NotAnInt()));
        }
        return delay(ms);
    }
};

int main() {
    CHECK(always_ready().get());
    void_().get();

    auto t0 = steady_clock::now();
    CHECK(sleep(200).get());
    CHECK(seconds_since(t0) >= 0.2);

    // Sequential.
    t0 = steady_clock::now();
    CHECK_EQ(say_after(100, "Alice").get(), "Hello, Alice!");
    CHECK_EQ(say_after(200, "Bob").get(), "Hello, Bob!");
    CHECK(seconds_since(t0) >= 0.3);

    // Concurrent: both run at once.
    t0 = steady_clock::now();
    auto alice = say_after(100, "Alice");
    auto bob = say_after(200, "Bob");
    CHECK_EQ(alice.get(), "Hello, Alice!");
    CHECK_EQ(bob.get(), "Hello, Bob!");
    CHECK(seconds_since(t0) >= 0.2 && seconds_since(t0) < 0.3);

    // Methods and constructors.
    auto megaphone = new_megaphone();
    t0 = steady_clock::now();
    CHECK_EQ(megaphone->say_after(200, "Alice").get(), "HELLO, ALICE!");
    CHECK(seconds_since(t0) >= 0.2);
    CHECK_EQ(Megaphone::secondary().get()->say_after(0, "Alice").get(), "HELLO, ALICE!");
    CHECK_EQ(UdlMegaphone::secondary().get()->say_after(0, "udl").get(), "HELLO, UDL!");

    // Async methods of Rust trait objects.
    for (const auto& traits : {get_say_after_traits()}) {
        t0 = steady_clock::now();
        CHECK_EQ(traits[0]->say_after(100, "Alice").get(), "Hello, Alice!");
        CHECK_EQ(traits[1]->say_after(100, "Bob").get(), "Hello, Bob!");
        CHECK(seconds_since(t0) >= 0.2);
    }
    auto udl_traits = get_say_after_udl_traits();
    CHECK_EQ(udl_traits[0]->say_after(100, "Alice").get(), "Hello, Alice!");
    CHECK_EQ(udl_traits[1]->say_after(100, "Bob").get(), "Hello, Bob!");

    // Async methods implemented in C++.
    auto parser = std::make_shared<CppAsyncParser>();
    CHECK_EQ(as_string_using_trait(parser, 1, 42).get(), "42");
    CHECK_EQ(try_from_string_using_trait(parser, 1, "42").get(), 42);
    CHECK_THROWS(try_from_string_using_trait(parser, 1, "fourty-two").get(), ParserError_NotAnInt);
    CHECK_THROWS(try_from_string_using_trait(parser, 1, "force-unexpected-exception").get(),
                 ParserError_UnexpectedError);
    delay_using_trait(parser, 1).get();
    try_delay_using_trait(parser, "1").get();
    CHECK_THROWS(try_delay_using_trait(parser, "one").get(), ParserError_NotAnInt);
    // Rust cancels the call: the C++ promise sees it and never completes the delay.
    int before = parser->completed_delays;
    cancel_delay_using_trait(parser, 10).get();
    std::this_thread::sleep_for(100ms);
    CHECK_EQ(parser->completed_delays.load(), before);

    // Objects as arguments, Tokio, errors.
    t0 = steady_clock::now();
    CHECK_EQ(say_after_with_megaphone(new_megaphone(), 200, "Alice").get(), "HELLO, ALICE!");
    CHECK(seconds_since(t0) >= 0.2);
    CHECK_EQ(say_after_with_tokio(200, "Alice").get(), "Hello, Alice (with Tokio)!");
    CHECK_EQ(fallible_me(false).get(), 42);
    CHECK_THROWS(fallible_me(true).get(), MyError);
    CHECK_EQ(megaphone->fallible_me(false).get(), 42);
    CHECK_THROWS(megaphone->fallible_me(true).get(), MyError);
    CHECK_EQ(fallible_struct(false).get()->fallible_me(false).get(), 42);
    CHECK_THROWS(fallible_struct(true).get(), MyError);

    auto record = new_my_record("foo", 42).get();
    CHECK_EQ(record.a, "foo");
    CHECK_EQ(record.b, 42u);

    // Cancelling a Rust future: get() throws at once instead of waiting for it.
    auto task = say_after(5000, "Alice");
    std::this_thread::sleep_for(50ms);
    t0 = steady_clock::now();
    task.cancel();
    CHECK_THROWS(task.get(), uniffi::CancelledError);
    CHECK(seconds_since(t0) < 1.0);

    // A cancelled future releases what it holds: the next call gets the lock.
    auto holder = use_shared_resource(SharedResourceOptions{5000, 100});
    std::this_thread::sleep_for(50ms);
    holder.cancel();
    use_shared_resource(SharedResourceOptions{0, 1000}).get();
    use_shared_resource(SharedResourceOptions{100, 1000}).get();
    use_shared_resource(SharedResourceOptions{0, 1000}).get();

    // Continuations instead of blocking.
    uniffi::Promise<std::string> got;
    say_after(10, "then").then([got](uniffi::Future<std::string> done) { got.set_value(done.get()); });
    CHECK_EQ(got.get_future().get(), "Hello, then!");

    // A custom executor runs the continuations.
    std::atomic<int> posted{0};
    uniffi::set_executor([&posted](std::function<void()> task) {
        ++posted;
        std::thread(std::move(task)).detach();
    });
    CHECK_EQ(say_after(10, "executor").get(), "Hello, executor!");
    CHECK(posted.load() > 0);
    uniffi::set_executor(nullptr);
    return check::report();
}
