// This file is part of uniffi-bindgen-cpp and is copied next to every generated
// binding. It holds everything that does not depend on the component: the C ABI
// structs, the wire-format reader/writer, the codecs for the builtin types, the
// handle map, the exception types and the async machinery.
//
// Requires C++17.
//
// Bindings for several libraries each bring a copy; the guard (not `#pragma once`,
// which goes by path) makes the copies one. Copies must come from the same release.
#ifndef UNIFFI_BINDGEN_CPP_RUNTIME_HPP
#define UNIFFI_BINDGEN_CPP_RUNTIME_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <future>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace uniffi {

// ---------------------------------------------------------------------------
// C ABI shared by every UniFFI component.
// ---------------------------------------------------------------------------

struct RustBuffer {
    uint64_t capacity;
    uint64_t len;
    uint8_t* data;
};

struct ForeignBytes {
    int32_t len;
    const uint8_t* data;
};

struct RustCallStatus {
    int8_t code;
    RustBuffer error_buf;
};

constexpr int8_t CALL_SUCCESS = 0;
constexpr int8_t CALL_ERROR = 1;
constexpr int8_t CALL_UNEXPECTED_ERROR = 2;
constexpr int8_t CALL_CANCELLED = 3;

constexpr int8_t RUST_FUTURE_POLL_READY = 0;
constexpr int8_t RUST_FUTURE_POLL_WAKE = 1;

// ---------------------------------------------------------------------------
// Exceptions.
// ---------------------------------------------------------------------------

/// Base class of every exception thrown by generated bindings.
class Exception : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// A UniFFI-level failure: corrupt buffer, stale handle, contract mismatch.
class InternalError : public Exception {
public:
    using Exception::Exception;
};

/// The Rust side panicked.
class PanicError : public Exception {
public:
    using Exception::Exception;
};

/// The Rust future was cancelled.
class CancelledError : public Exception {
public:
    CancelledError() : Exception("the call was cancelled") {}
};

// ---------------------------------------------------------------------------
// Wire format: big-endian, written by `Writer`, consumed by `Reader`.
// ---------------------------------------------------------------------------

class Reader {
public:
    Reader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    template <class T>
    T read_int() {
        static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>);
        need(sizeof(T));
        using U = std::make_unsigned_t<T>;
        U value = 0;
        for (size_t i = 0; i < sizeof(T); ++i) {
            value = static_cast<U>((value << 8) | data_[offset_ + i]);
        }
        offset_ += sizeof(T);
        return static_cast<T>(value);
    }

    float read_float() {
        uint32_t bits = read_int<uint32_t>();
        float out;
        std::memcpy(&out, &bits, sizeof out);
        return out;
    }

    double read_double() {
        uint64_t bits = read_int<uint64_t>();
        double out;
        std::memcpy(&out, &bits, sizeof out);
        return out;
    }

    void read_bytes(void* dst, size_t count) {
        need(count);
        if (count != 0) {
            std::memcpy(dst, data_ + offset_, count);
        }
        offset_ += count;
    }

    bool has_remaining() const { return offset_ < size_; }

private:
    void need(size_t count) const {
        if (size_ - offset_ < count) {
            throw InternalError("reading the requested value would read past the end of the buffer");
        }
    }

    const uint8_t* data_;
    size_t size_;
    size_t offset_ = 0;
};

class Writer {
public:
    template <class T>
    void write_int(T value) {
        static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>);
        using U = std::make_unsigned_t<T>;
        U v = static_cast<U>(value);
        for (size_t i = sizeof(T); i > 0; --i) {
            buf_.push_back(static_cast<uint8_t>((v >> ((i - 1) * 8)) & 0xff));
        }
    }

    void write_float(float value) {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof bits);
        write_int(bits);
    }

    void write_double(double value) {
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof bits);
        write_int(bits);
    }

    void write_bytes(const void* src, size_t count) {
        const auto* p = static_cast<const uint8_t*>(src);
        buf_.insert(buf_.end(), p, p + count);
    }

    const std::vector<uint8_t>& bytes() const { return buf_; }

private:
    std::vector<uint8_t> buf_;
};

// ---------------------------------------------------------------------------
// Codecs: `Codec<T>::read` / `Codec<T>::write` for every type that can sit
// inside a buffer. Generated code specializes it for records, enums, errors,
// objects and custom types.
// ---------------------------------------------------------------------------

template <class T, class Enable = void>
struct Codec;

template <class T>
struct Codec<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>> {
    static T read(Reader& r) { return r.read_int<T>(); }
    static void write(Writer& w, T v) { w.write_int<T>(v); }
};

template <>
struct Codec<bool> {
    static bool read(Reader& r) {
        return r.read_int<int8_t>() != 0;
    }
    static void write(Writer& w, bool v) { w.write_int<int8_t>(v ? 1 : 0); }
};

template <>
struct Codec<float> {
    static float read(Reader& r) { return r.read_float(); }
    static void write(Writer& w, float v) { w.write_float(v); }
};

template <>
struct Codec<double> {
    static double read(Reader& r) { return r.read_double(); }
    static void write(Writer& w, double v) { w.write_double(v); }
};

template <>
struct Codec<std::string> {
    static std::string read(Reader& r) {
        int32_t len = r.read_int<int32_t>();
        if (len < 0) {
            throw InternalError("negative string length");
        }
        std::string out(static_cast<size_t>(len), '\0');
        r.read_bytes(out.data(), out.size());
        return out;
    }
    static void write(Writer& w, const std::string& v) {
        w.write_int<int32_t>(static_cast<int32_t>(v.size()));
        w.write_bytes(v.data(), v.size());
    }
};

template <class T>
struct Codec<std::vector<T>> {
    static std::vector<T> read(Reader& r) {
        int32_t len = r.read_int<int32_t>();
        if (len < 0) {
            throw InternalError("negative sequence length");
        }
        std::vector<T> out;
        if constexpr (std::is_same_v<T, uint8_t>) {
            out.resize(static_cast<size_t>(len));
            r.read_bytes(out.data(), out.size());
        } else {
            out.reserve(static_cast<size_t>(len));
            for (int32_t i = 0; i < len; ++i) {
                out.push_back(Codec<T>::read(r));
            }
        }
        return out;
    }
    static void write(Writer& w, const std::vector<T>& v) {
        w.write_int<int32_t>(static_cast<int32_t>(v.size()));
        if constexpr (std::is_same_v<T, uint8_t>) {
            w.write_bytes(v.data(), v.size());
        } else {
            for (const auto& item : v) {
                Codec<T>::write(w, item);
            }
        }
    }
};

template <class T>
struct Codec<std::optional<T>> {
    static std::optional<T> read(Reader& r) {
        int8_t tag = r.read_int<int8_t>();
        if (tag == 0) {
            return std::nullopt;
        }
        if (tag == 1) {
            return Codec<T>::read(r);
        }
        throw InternalError("unexpected optional tag; should be 0 or 1");
    }
    static void write(Writer& w, const std::optional<T>& v) {
        if (!v) {
            w.write_int<int8_t>(0);
        } else {
            w.write_int<int8_t>(1);
            Codec<T>::write(w, *v);
        }
    }
};

template <class K, class V>
struct Codec<std::map<K, V>> {
    static std::map<K, V> read(Reader& r) {
        int32_t len = r.read_int<int32_t>();
        if (len < 0) {
            throw InternalError("negative map length");
        }
        std::map<K, V> out;
        for (int32_t i = 0; i < len; ++i) {
            K key = Codec<K>::read(r);
            V value = Codec<V>::read(r);
            out.emplace(std::move(key), std::move(value));
        }
        return out;
    }
    static void write(Writer& w, const std::map<K, V>& v) {
        w.write_int<int32_t>(static_cast<int32_t>(v.size()));
        for (const auto& [key, value] : v) {
            Codec<K>::write(w, key);
            Codec<V>::write(w, value);
        }
    }
};

/// A point in time with nanosecond resolution, relative to the Unix epoch.
using Timestamp =
    std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;

/// A span of time with nanosecond resolution.
using Duration = std::chrono::nanoseconds;

template <>
struct Codec<Timestamp> {
    static Timestamp read(Reader& r) {
        int64_t secs = r.read_int<int64_t>();
        uint32_t nanos = r.read_int<uint32_t>();
        using ns = std::chrono::nanoseconds;
        if (secs >= 0) {
            return Timestamp(ns(secs * 1000000000LL + static_cast<int64_t>(nanos)));
        }
        // Before the epoch the seconds are negated and the nanoseconds stay positive.
        return Timestamp(ns(secs * 1000000000LL - static_cast<int64_t>(nanos)));
    }
    static void write(Writer& w, const Timestamp& v) {
        int64_t total = v.time_since_epoch().count();
        int64_t secs = total / 1000000000LL;
        int64_t nanos = total % 1000000000LL;
        if (nanos < 0) {
            nanos = -nanos;
        }
        w.write_int<int64_t>(secs);
        w.write_int<uint32_t>(static_cast<uint32_t>(nanos));
    }
};

template <>
struct Codec<Duration> {
    static Duration read(Reader& r) {
        uint64_t secs = r.read_int<uint64_t>();
        uint32_t nanos = r.read_int<uint32_t>();
        constexpr uint64_t max_secs =
            static_cast<uint64_t>(std::numeric_limits<int64_t>::max() / 1000000000LL) - 1;
        if (secs > max_secs) {
            return Duration::max();
        }
        return Duration(static_cast<int64_t>(secs) * 1000000000LL + static_cast<int64_t>(nanos));
    }
    static void write(Writer& w, const Duration& v) {
        int64_t total = v.count() < 0 ? 0 : v.count();
        w.write_int<uint64_t>(static_cast<uint64_t>(total / 1000000000LL));
        w.write_int<uint32_t>(static_cast<uint32_t>(total % 1000000000LL));
    }
};

// ---------------------------------------------------------------------------
// Handle maps: how foreign objects are passed to Rust. Handles are odd so that
// they can be told apart from Rust handles (which are even).
// ---------------------------------------------------------------------------

template <class T>
class HandleMap {
public:
    uint64_t insert(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        return do_insert(std::move(value));
    }

    T get(uint64_t handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(handle);
        if (it == map_.end()) {
            throw InternalError("the object in the handle map has been dropped already");
        }
        return it->second;
    }

    uint64_t clone(uint64_t handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(handle);
        if (it == map_.end()) {
            throw InternalError("the object in the handle map has been dropped already");
        }
        T copy = it->second;
        return do_insert(std::move(copy));
    }

    T remove(uint64_t handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(handle);
        if (it == map_.end()) {
            throw InternalError("the object in the handle map has been dropped already");
        }
        T out = std::move(it->second);
        map_.erase(it);
        return out;
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return map_.size();
    }

private:
    uint64_t do_insert(T value) {
        uint64_t handle = next_;
        next_ += 2;
        map_.emplace(handle, std::move(value));
        return handle;
    }

    std::mutex mutex_;
    std::unordered_map<uint64_t, T> map_;
    uint64_t next_ = 1;
};

/// Marks the constructor generated code uses to wrap a Rust handle.
struct HandleTag {};

// ---------------------------------------------------------------------------
// Value equality: what the field-wise `==` of records and enums uses. Objects
// are compared through their own `==` (the Rust `Eq`) when they have one, and by
// identity otherwise; containers compare their elements the same way.
// ---------------------------------------------------------------------------

template <class T, class = void>
struct HasEqual : std::false_type {};

template <class T>
struct HasEqual<T, std::void_t<decltype(std::declval<const T&>() == std::declval<const T&>())>>
    : std::true_type {};

template <class T>
bool equal(const T& a, const T& b) {
    return a == b;
}

template <class T>
bool equal(const std::shared_ptr<T>& a, const std::shared_ptr<T>& b) {
    if (a == b) {
        return true;
    }
    if constexpr (HasEqual<T>::value) {
        return a && b && *a == *b;
    } else {
        return false;
    }
}

template <class T>
bool equal(const std::optional<T>& a, const std::optional<T>& b) {
    if (a.has_value() != b.has_value()) {
        return false;
    }
    return !a.has_value() || equal(*a, *b);
}

template <class T>
bool equal(const std::vector<T>& a, const std::vector<T>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (!equal(a[i], b[i])) {
            return false;
        }
    }
    return true;
}

template <class K, class V>
bool equal(const std::map<K, V>& a, const std::map<K, V>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (auto ia = a.begin(), ib = b.begin(); ia != a.end(); ++ia, ++ib) {
        if (!(ia->first == ib->first) || !equal(ia->second, ib->second)) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Async.
//
// A Rust `async fn` returns a `uniffi::Future<T>`: wait for it with `get()`, attach a
// continuation with `then()`, cancel it with `cancel()`, or `co_await` it in C++20.
// An async method of a callback interface returns a `uniffi::Future<T>` as well;
// make one with `uniffi::Promise<T>` or `uniffi::Future<T>::ready()`.
//
// Continuations (the polling of Rust futures and the callbacks given to `then()`)
// run on the executor. By default that is one background thread; hand them to your
// own event loop with `uniffi::set_executor()`.
// ---------------------------------------------------------------------------

/// Runs a task, now or later, on any thread. Must not run it inline if the caller
/// holds locks the task needs; posting to a queue is always safe.
using Executor = std::function<void(std::function<void()>)>;

namespace detail {

/// The default executor: one background thread, leaked on purpose so that it
/// outlives static destruction.
class DefaultExecutor {
public:
    static DefaultExecutor& instance() {
        static DefaultExecutor* executor = new DefaultExecutor();
        return *executor;
    }

    void post(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(std::move(task));
        }
        cv_.notify_one();
    }

private:
    DefaultExecutor() : thread_([this] { run(); }) { thread_.detach(); }

    void run() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return !queue_.empty(); });
                task = std::move(queue_.front());
                queue_.pop_front();
            }
            task();
        }
    }

    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::function<void()>> queue_;
    std::thread thread_;
};

struct ExecutorSlot {
    std::mutex mutex;
    Executor executor;
};

inline ExecutorSlot& executor_slot() {
    static ExecutorSlot* slot = new ExecutorSlot();
    return *slot;
}

inline void post(std::function<void()> task) {
    Executor executor;
    {
        std::lock_guard<std::mutex> lock(executor_slot().mutex);
        executor = executor_slot().executor;
    }
    if (executor) {
        executor(std::move(task));
    } else {
        DefaultExecutor::instance().post(std::move(task));
    }
}

/// What a `Future` and its producer share.
template <class T>
struct SharedState {
    using Value = std::conditional_t<std::is_void_v<T>, std::monostate, T>;

    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
    bool cancel_requested = false;
    std::optional<Value> value;
    std::exception_ptr error;
    std::vector<std::function<void()>> on_done;
    /// Set by the producer: stops the work when the consumer cancels.
    std::function<void()> on_cancel;

    void finish(std::optional<Value> v, std::exception_ptr e) {
        std::vector<std::function<void()>> callbacks;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (done) {
                return;
            }
            done = true;
            value = std::move(v);
            error = std::move(e);
            callbacks.swap(on_done);
            on_cancel = nullptr;
        }
        cv.notify_all();
        for (auto& callback : callbacks) {
            post(std::move(callback));
        }
    }

    void request_cancel() {
        std::function<void()> cancel;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (done || cancel_requested) {
                return;
            }
            cancel_requested = true;
            cancel = on_cancel;
        }
        if (cancel) {
            cancel();
        }
    }
};

}  // namespace detail

/// Sets the executor that runs async continuations; an empty one restores the
/// default background thread.
inline void set_executor(Executor executor) {
    std::lock_guard<std::mutex> lock(detail::executor_slot().mutex);
    detail::executor_slot().executor = std::move(executor);
}

template <class T>
class Promise;

/// The eventual result of an async call. Move-only, like `std::future`.
template <class T>
class Future {
public:
    Future() = default;

    /// A future that is already complete.
    template <class U = T, class = std::enable_if_t<!std::is_void_v<U>>>
    static Future ready(U value) {
        Future future(std::make_shared<detail::SharedState<T>>());
        future.state_->finish(std::move(value), nullptr);
        return future;
    }

    /// A completed `Future<void>`.
    template <class U = T, class = std::enable_if_t<std::is_void_v<U>>>
    static Future ready() {
        Future future(std::make_shared<detail::SharedState<T>>());
        future.state_->finish(std::monostate{}, nullptr);
        return future;
    }

    /// A future that failed with `error`.
    static Future failed(std::exception_ptr error) {
        Future future(std::make_shared<detail::SharedState<T>>());
        future.state_->finish(std::nullopt, std::move(error));
        return future;
    }

    /// Adopts a `std::future`. Waiting for it takes a thread of its own until it is
    /// ready; prefer `uniffi::Promise` where you can complete the result directly.
    static Future from(std::future<T> future) {
        Promise<T> promise;
        Future result = promise.get_future();
        std::thread([promise, future = std::move(future)]() mutable {
            try {
                if constexpr (std::is_void_v<T>) {
                    future.get();
                    promise.set_value();
                } else {
                    promise.set_value(future.get());
                }
            } catch (...) {
                promise.set_exception(std::current_exception());
            }
        }).detach();
        return result;
    }

    bool valid() const { return state_ != nullptr; }

    bool is_ready() const {
        std::lock_guard<std::mutex> lock(state().mutex);
        return state().done;
    }

    void wait() const {
        std::unique_lock<std::mutex> lock(state().mutex);
        state().cv.wait(lock, [this] { return state().done; });
    }

    template <class Rep, class Period>
    std::future_status wait_for(const std::chrono::duration<Rep, Period>& timeout) const {
        std::unique_lock<std::mutex> lock(state().mutex);
        bool done = state().cv.wait_for(lock, timeout, [this] { return state().done; });
        return done ? std::future_status::ready : std::future_status::timeout;
    }

    /// Waits for the result and returns it, or throws the error. Like
    /// `std::future::get()` it can be called once.
    T get() {
        wait();
        auto state = std::move(state_);
        if (state->error) {
            std::rethrow_exception(state->error);
        }
        if constexpr (!std::is_void_v<T>) {
            return std::move(*state->value);
        }
    }

    /// Asks the producer to stop. A Rust future is dropped and `get()` throws
    /// `uniffi::CancelledError`; a no-op once the result is there.
    void cancel() {
        if (state_) {
            state_->request_cancel();
        }
    }

    /// Calls `callback(Future<T>)` with the completed future, on the executor.
    template <class F>
    void then(F callback) {
        auto state = std::move(state_);
        auto run = [state, callback = std::move(callback)]() mutable {
            callback(Future(state));
        };
        std::unique_lock<std::mutex> lock(state->mutex);
        if (state->done) {
            lock.unlock();
            detail::post(std::move(run));
        } else {
            state->on_done.push_back(std::move(run));
        }
    }

#if defined(__cpp_impl_coroutine) && __cpp_impl_coroutine >= 201902L
    bool await_ready() const { return is_ready(); }
    template <class Handle>
    void await_suspend(Handle handle) {
        auto state = state_;
        std::unique_lock<std::mutex> lock(state->mutex);
        if (state->done) {
            lock.unlock();
            detail::post([handle] { handle.resume(); });
        } else {
            state->on_done.push_back([handle] { handle.resume(); });
        }
    }
    T await_resume() { return get(); }
#endif

    /// For generated code: the shared state.
    explicit Future(std::shared_ptr<detail::SharedState<T>> state) : state_(std::move(state)) {}
    const std::shared_ptr<detail::SharedState<T>>& uniffi_state() const { return state_; }

private:
    detail::SharedState<T>& state() const {
        if (!state_) {
            throw std::future_error(std::future_errc::no_state);
        }
        return *state_;
    }

    std::shared_ptr<detail::SharedState<T>> state_;
};

/// Completes a `uniffi::Future`: what an async callback implementation returns.
/// Copyable; all copies complete the same future.
template <class T>
class Promise {
public:
    Promise() : state_(std::make_shared<detail::SharedState<T>>()) {}

    Future<T> get_future() const { return Future<T>(state_); }

    template <class U = T, class = std::enable_if_t<!std::is_void_v<U>>>
    void set_value(U value) const {
        state_->finish(std::move(value), nullptr);
    }

    template <class U = T, class = std::enable_if_t<std::is_void_v<U>>>
    void set_value() const {
        state_->finish(std::monostate{}, nullptr);
    }

    void set_exception(std::exception_ptr error) const { state_->finish(std::nullopt, std::move(error)); }

    /// True once the consumer (for a callback, Rust) no longer wants the result.
    bool is_cancelled() const {
        std::lock_guard<std::mutex> lock(state_->mutex);
        return state_->cancel_requested;
    }

    /// Runs `callback` when the consumer cancels; at once if it already has.
    void on_cancel(std::function<void()> callback) const {
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            if (!state_->cancel_requested) {
                state_->on_cancel = std::move(callback);
                return;
            }
        }
        callback();
    }

private:
    std::shared_ptr<detail::SharedState<T>> state_;
};

namespace detail {

using PollFn = void (*)(uint64_t, void (*)(uint64_t, int8_t), uint64_t);
using HandleFn = void (*)(uint64_t);

/// A Rust future being polled. `freed` guards `cancel` against a freed handle.
struct RustFuture {
    uint64_t handle;
    PollFn poll;
    HandleFn cancel;
    HandleFn free_fn;
    std::function<void()> finish;
    std::mutex mutex;
    bool freed = false;
};

inline void poll_step(const std::shared_ptr<RustFuture>& future);

inline void on_continuation(uint64_t data, int8_t poll_result) {
    auto* raw = reinterpret_cast<std::shared_ptr<RustFuture>*>(data);
    post([raw, poll_result] {
        std::unique_ptr<std::shared_ptr<RustFuture>> owner(raw);
        std::shared_ptr<RustFuture> future = *owner;
        if (poll_result == RUST_FUTURE_POLL_READY) {
            future->finish();
            std::lock_guard<std::mutex> lock(future->mutex);
            future->free_fn(future->handle);
            future->freed = true;
        } else {
            poll_step(future);
        }
    });
}

inline void poll_step(const std::shared_ptr<RustFuture>& future) {
    auto* raw = new std::shared_ptr<RustFuture>(future);
    future->poll(future->handle, &on_continuation, reinterpret_cast<uint64_t>(raw));
}

/// Starts driving the Rust future `handle`. `complete` runs once it is ready, on
/// the executor, and produces the result or throws.
template <class R, class Complete>
Future<R> start_future(uint64_t handle, PollFn poll, HandleFn cancel, HandleFn free_fn, Complete complete) {
    auto state = std::make_shared<SharedState<R>>();
    auto future = std::make_shared<RustFuture>();
    future->handle = handle;
    future->poll = poll;
    future->cancel = cancel;
    future->free_fn = free_fn;
    future->finish = [state, complete, handle]() mutable {
        try {
            if constexpr (std::is_void_v<R>) {
                complete(handle);
                state->finish(std::monostate{}, nullptr);
            } else {
                state->finish(complete(handle), nullptr);
            }
        } catch (...) {
            state->finish(std::nullopt, std::current_exception());
        }
    };
    std::weak_ptr<RustFuture> weak = future;
    state->on_cancel = [weak] {
        if (auto f = weak.lock()) {
            std::lock_guard<std::mutex> lock(f->mutex);
            if (!f->freed) {
                f->cancel(f->handle);
            }
        }
    };
    post([future] { poll_step(future); });
    return Future<R>(state);
}

}  // namespace detail
}  // namespace uniffi

#endif  // UNIFFI_BINDGEN_CPP_RUNTIME_HPP
