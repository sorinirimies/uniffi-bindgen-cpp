// C++20: uniffi::Future is awaitable, in either direction.
#include <coroutine>
#include <exception>

#include "check.hpp"
#include "futures.hpp"

using namespace futures;

/// The smallest coroutine type: runs eagerly, reports through a promise.
struct Task {
    struct promise_type {
        uniffi::Promise<void> done;
        Task get_return_object() { return Task{done.get_future()}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() { done.set_value(); }
        void unhandled_exception() { done.set_exception(std::current_exception()); }
    };
    uniffi::Future<void> finished;
};

Task scenario() {
    CHECK_EQ(co_await say_after(10, "Alice"), "Hello, Alice!");
    auto megaphone = co_await Megaphone::secondary();
    CHECK_EQ(co_await megaphone->say_after(10, "Bob"), "HELLO, BOB!");
    bool threw = false;
    try {
        co_await fallible_me(true);
    } catch (const MyError&) {
        threw = true;
    }
    CHECK(threw);
}

int main() {
    scenario().finished.get();
    return check::report();
}
