// Port of uniffi-rs v0.32.2 examples/remote-types/tests/bindings/test_remote_types.py
#include "check.hpp"
#include "remote_types.hpp"

using namespace remote_types;

int main() {
    auto logger = LogSink::new_("SomeFile");
    logger->log(LogLevel::Info, "Hello world");
    // anyhow::Error is exported as an object, and used as an error type.
    CHECK_THROWS(LogSink::new_(""), AnyhowError);
    return check::report();
}
