// Port of uniffi-rs v0.32.2 fixtures/ext-types/http-headermap/tests/bindings/test_http_headermap.py
#include "check.hpp"
#include "ext_types_http_headermap.hpp"

using namespace ext_types_http_headermap;

class TestCallback : public HeaderMapCallback {
public:
    HeaderMap run(const HeaderMap& header_map) override {
        saw_map = header_map;
        auto copy = header_map;
        copy.push_back(HttpHeader{"from-callback", "hello"});
        return copy;
    }
    HeaderMap saw_map;
};

int main() {
    CHECK(get_headermap("Second value") ==
          (HeaderMap{HttpHeader{"test-header", "First value"}, HttpHeader{"test-header", "Second value"}}));
    auto callback = std::make_shared<TestCallback>();
    CHECK(run_callback(callback) == (HeaderMap{HttpHeader{"foo", "bar"}, HttpHeader{"from-callback", "hello"}}));
    CHECK(callback->saw_map == HeaderMap{HttpHeader{"foo", "bar"}});
    return check::report();
}
