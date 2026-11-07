// Port of uniffi-rs v0.31.2 fixtures/regressions/logging-callback-interface/tests/bindings/test.py
#include <algorithm>
#include <mutex>
#include <vector>

#include "check.hpp"
#include "regression_logging_callback_interface.hpp"

using namespace regression_logging_callback_interface;

class CollectingLogger : public Logger {
public:
    void log_message(const std::string& message) override {
        std::lock_guard<std::mutex> lock(mutex);
        messages.push_back(message);
    }
    std::mutex mutex;
    std::vector<std::string> messages;
};

int main() {
    auto logger = std::make_shared<CollectingLogger>();
    install_logger(logger);
    CHECK(logger->messages.empty());
    log_something();
    std::lock_guard<std::mutex> lock(logger->mutex);
    CHECK(std::find(logger->messages.begin(), logger->messages.end(), "something") != logger->messages.end());
    return check::report();
}
