// End-to-end test of the generated bindings against the `tour` fixture.
#include <cmath>
#include <cstdio>
#include <iostream>

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

#define CHECK_THROWS(type, expr)                                                 \
    do {                                                                         \
        ++checks;                                                                \
        bool caught = false;                                                     \
        try {                                                                    \
            expr;                                                                \
        } catch (const type&) {                                                  \
            caught = true;                                                       \
        } catch (...) {                                                          \
        }                                                                        \
        if (!caught) {                                                           \
            ++failures;                                                          \
            std::fprintf(stderr, "%s:%d: expected %s from %s\n", __FILE__, __LINE__, #type, #expr); \
        }                                                                        \
    } while (0)

using namespace std::chrono_literals;

static void test_primitives() {
    CHECK(tour::add_u8(200, 100) == 44);
    CHECK(tour::add_i32(-5, 3) == -2);
    CHECK(tour::add_u64(1ULL << 40, 5) == (1ULL << 40) + 5);
    CHECK(tour::negate_i64(9) == -9);
    CHECK(tour::half_f32(3.0f) == 1.5f);
    CHECK(tour::half_f64(5.0) == 2.5);
    CHECK(tour::invert(true) == false);
    CHECK(tour::invert(false) == true);
    CHECK(tour::shout("héllo") == "HÉLLO");
    CHECK(tour::shout("") == "");
    CHECK((tour::reverse_bytes({1, 2, 3}) == std::vector<uint8_t>{3, 2, 1}));
    tour::nothing();
    CHECK(tour::greet("Ada") == "Hello, Ada!");
    CHECK(tour::greet("Ada", "Hi") == "Hi, Ada!");
}

static void test_containers() {
    CHECK(tour::maybe_double(4) == std::optional<int32_t>(8));
    CHECK(!tour::maybe_double(std::nullopt).has_value());
    CHECK(tour::sum({1, 2, 3, 4}) == 10);
    CHECK(tour::sum({}) == 0);
    auto lengths = tour::word_lengths({"a", "bcd", "ef"});
    CHECK(lengths.size() == 3 && lengths["bcd"] == 3);
    std::vector<std::optional<std::vector<std::string>>> n = {std::nullopt, std::vector<std::string>{"x", "y"}, std::vector<std::string>{}};
    CHECK(tour::nested(n) == n);
    std::map<std::string, std::vector<uint64_t>> m = {{"a", {1, 2}}, {"b", {}}};
    CHECK(tour::echo_u64s(m) == m);

    uniffi::Timestamp epoch{};
    auto t = tour::timestamp_plus(epoch + std::chrono::seconds(100), 2500ms);
    CHECK(t.time_since_epoch() == std::chrono::milliseconds(102500));
    auto before = tour::timestamp_plus(epoch - std::chrono::seconds(10), 4s);
    CHECK(before.time_since_epoch() == std::chrono::seconds(-6));
    CHECK(tour::double_duration(1500ms) == 3s);
}

static void test_records_and_enums() {
    CHECK(tour::mix(tour::Color::Red) == tour::Color::Green);
    CHECK(tour::mix(tour::Color::Blue) == tour::Color::Red);

    auto circle = tour::make_shape(0);
    const auto* c = circle.get_if<tour::Shape::Circle>();
    CHECK(c != nullptr && c->radius == 2.0 && c->center.y == 2.0);
    auto rect = tour::make_shape(1);
    const auto* r = rect.get_if<tour::Shape::Rect>();
    CHECK(r != nullptr && r->v1.x == 2.0);
    CHECK(tour::make_shape(2).is<tour::Shape::Empty>());
    CHECK(tour::area(circle) == 12.0);
    CHECK(tour::area(rect) == 6.0);
    CHECK(tour::area(tour::Shape::Empty{}) == 0.0);
    CHECK(tour::area(tour::Shape::Circle{{0, 0}, 1.0}) == 3.0);

    tour::Settings s;
    s.name = "n";
    s.origin = {7.0, 8.0};
    CHECK(tour::describe(s) == "n:8:None::7");
    s.retries = 2;
    s.label = "L";
    s.tags = {"a", "b"};
    CHECK(tour::describe(s) == "n:2:Some(\"L\"):a+b:7");
}

static void test_errors() {
    tour::fail_data(0);
    try {
        tour::fail_data(1);
        CHECK(false);
    } catch (const tour::DataError_BadCode& e) {
        CHECK(e.code == 42);
    }
    try {
        tour::fail_data(2);
        CHECK(false);
    } catch (const tour::DataError& e) {
        CHECK(e.uniffi_variant_index() == 2);
        CHECK(dynamic_cast<const tour::DataError_TooLong&>(e).v0 == 9);
    }
    CHECK_THROWS(tour::DataError_Plain, tour::fail_data(3));
    CHECK_THROWS(uniffi::Exception, tour::fail_data(3));

    CHECK(tour::fail_flat(0) == 7);
    CHECK_THROWS(tour::FlatError_NotFound, tour::fail_flat(1));
    try {
        tour::fail_flat(2);
    } catch (const tour::FlatError_Denied& e) {
        CHECK(e.v0 == "nope");
    }

    try {
        tour::fail_message(1);
        CHECK(false);
    } catch (const tour::MessageError_Io& e) {
        CHECK(e.message == "io failure: disk");
    }
    CHECK_THROWS(tour::MessageError_Closed, tour::fail_message(2));

    try {
        tour::panics();
        CHECK(false);
    } catch (const uniffi::PanicError& e) {
        CHECK(std::string(e.what()).find("boom") != std::string::npos);
    }
}

static void test_objects() {
    auto counter = tour::Counter::new_(10);
    CHECK(counter->get() == 10);
    CHECK(counter->add(5) == 15);
    CHECK(tour::Counter::zero()->get() == 0);
    CHECK_THROWS(tour::FlatError_Denied, tour::Counter::checked(-1));
    CHECK(tour::Counter::checked(3)->get() == 3);

    auto other = tour::Counter::new_(100);
    counter->merge(other);
    CHECK(counter->get() == 115);
    CHECK(other->get() == 100);  // `other` is still usable after being passed to Rust

    auto twin = counter->twin();
    CHECK(twin->get() == 115 && twin != counter);
    CHECK(*twin == *counter);
    CHECK(counter->maybe_twin(true).has_value());
    CHECK(!counter->maybe_twin(false).has_value());
    CHECK(counter->to_string() == "Counter(115)");
    CHECK(counter->hash() == twin->hash());

    CHECK(counter->try_sub(15) == 100);
    CHECK_THROWS(tour::DataError_BadCode, counter->try_sub(1000));

    auto holder = tour::make_holder(1);
    CHECK(holder.counter->get() == 1);
    CHECK(holder.others.size() == 2 && holder.others[1]->get() == 3);
}

class RecordingListener : public tour::Listener {
public:
    int32_t on_event(const std::string& name, int32_t value) override {
        names.push_back(name);
        return value * 10;
    }
    void on_done() override { done = true; }
    std::string fallible(int32_t n) override {
        if (n < 0) {
            throw tour::FlatError_Denied("negative");
        }
        if (n == 0) {
            throw std::runtime_error("zero");
        }
        return "ok" + std::to_string(n);
    }
    std::vector<std::string> names;
    bool done = false;
};

static void test_callbacks() {
    auto listener = std::make_shared<RecordingListener>();
    CHECK(tour::notify(listener, 4) == 60);
    CHECK(listener->done);
    CHECK(listener->names.size() == 4 && listener->names[3] == "event-3");

    CHECK(tour::try_listener(listener, 5) == "ok5");
    CHECK_THROWS(tour::FlatError_Denied, tour::try_listener(listener, -1));
    CHECK_THROWS(uniffi::PanicError, tour::try_listener(listener, 0));
}

class CppGreeter : public tour::Greeter {
public:
    std::string greet(const std::string& name) override { return "c++ says hi to " + name; }
};

static void test_trait_objects() {
    auto rust = tour::rust_greeter();
    CHECK(rust->greet("x") == "rust says hi to x");
    CHECK(tour::use_greeter(rust, "y") == "rust says hi to y");
    auto cpp = std::make_shared<CppGreeter>();
    CHECK(tour::use_greeter(cpp, "z") == "c++ says hi to z");
}

class CppSource : public tour::AsyncSource {
public:
    uniffi::Future<std::string> fetch(const std::string& key) override {
        return uniffi::Future<std::string>::from(std::async(std::launch::async, [key] {
            std::this_thread::sleep_for(20ms);
            return "fetched " + key;
        }));
    }
};

static void test_async() {
    CHECK(tour::slow_add(2, 3).get() == 5);
    CHECK(tour::slow_fail(false).get() == "fine");
    CHECK_THROWS(tour::DataError_Plain, (void)tour::slow_fail(true).get());
    tour::slow_unit().get();

    auto worker = tour::Worker::new_("w");
    CHECK(worker->work(3).get() == "w did 3");

    // Many at once.
    std::vector<uniffi::Future<int32_t>> futures;
    for (int i = 0; i < 50; ++i) {
        futures.push_back(tour::slow_add(i, i));
    }
    int ok = 0;
    for (int i = 0; i < 50; ++i) {
        ok += futures[i].get() == i * 2;
    }
    CHECK(ok == 50);

    CHECK(tour::fetch_via(std::make_shared<CppSource>(), "k").get() == "fetched k");
}

static void test_custom_types() {
    CHECK(tour::next_handle(41) == 42);
    CHECK(tour::host_of("https://example.com/a/b") == "example.com");
}

int main() {
    test_primitives();
    test_containers();
    test_records_and_enums();
    test_errors();
    test_objects();
    test_callbacks();
    test_trait_objects();
    test_async();
    test_custom_types();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
