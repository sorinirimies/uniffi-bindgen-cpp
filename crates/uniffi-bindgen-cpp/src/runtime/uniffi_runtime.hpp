// This file is part of uniffi-bindgen-cpp and is copied next to every generated
// binding. It holds everything that does not depend on the component: the C ABI
// structs, the wire-format reader/writer, the codecs for the builtin types, the
// handle map, the exception types and the async machinery.
//
// Requires C++17.
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
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
// Async: Rust futures become `std::future`. A single background thread polls
// them, so a wake-up from a Rust thread never re-enters Rust.
// ---------------------------------------------------------------------------

namespace detail {

class Executor {
public:
    static Executor& instance() {
        // Leaked on purpose: the worker thread outlives static destruction.
        static Executor* executor = new Executor();
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
    Executor() : thread_([this] { run(); }) { thread_.detach(); }

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

using PollFn = void (*)(uint64_t, void (*)(uint64_t, int8_t), uint64_t);
using FreeFn = void (*)(uint64_t);

struct FutureTask {
    uint64_t handle;
    PollFn poll;
    FreeFn free_fn;
    std::function<void()> finish;
};

inline void poll_step(const std::shared_ptr<FutureTask>& task);

inline void on_continuation(uint64_t data, int8_t poll_result) {
    auto* raw = reinterpret_cast<std::shared_ptr<FutureTask>*>(data);
    Executor::instance().post([raw, poll_result] {
        std::unique_ptr<std::shared_ptr<FutureTask>> owner(raw);
        std::shared_ptr<FutureTask> task = *owner;
        if (poll_result == RUST_FUTURE_POLL_READY) {
            task->finish();
            task->free_fn(task->handle);
        } else {
            poll_step(task);
        }
    });
}

inline void poll_step(const std::shared_ptr<FutureTask>& task) {
    auto* raw = new std::shared_ptr<FutureTask>(task);
    task->poll(task->handle, &on_continuation, reinterpret_cast<uint64_t>(raw));
}

/// Start driving the Rust future `handle`. `complete` runs once the future is
/// ready, on the executor thread, and produces the result or throws.
template <class R, class Complete>
std::future<R> start_future(uint64_t handle, PollFn poll, FreeFn free_fn, Complete complete) {
    auto promise = std::make_shared<std::promise<R>>();
    std::future<R> future = promise->get_future();
    auto task = std::make_shared<FutureTask>();
    task->handle = handle;
    task->poll = poll;
    task->free_fn = free_fn;
    task->finish = [promise, complete, handle]() mutable {
        try {
            if constexpr (std::is_void_v<R>) {
                complete(handle);
                promise->set_value();
            } else {
                promise->set_value(complete(handle));
            }
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    };
    Executor::instance().post([task] { poll_step(task); });
    return future;
}

/// Run `fn` on its own thread. Used to wait for futures returned by foreign
/// implementations of async callback methods without blocking the executor.
inline void spawn(std::function<void()> fn) {
    std::thread(std::move(fn)).detach();
}

}  // namespace detail
}  // namespace uniffi
