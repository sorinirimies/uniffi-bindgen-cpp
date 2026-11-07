// Port of uniffi-rs v0.32.2 examples/async-api-client/tests/bindings/test_async_api_client.py
#include <thread>

#include "async_api_client.hpp"
#include "check.hpp"

using namespace async_api_client;

class CppHttpClient : public HttpClient {
public:
    uniffi::Future<std::string> fetch(const std::string& url, const std::string& credentials) override {
        uniffi::Promise<std::string> promise;
        std::thread([promise, url, credentials] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (credentials != "username:password") {
                promise.set_exception(std::make_exception_ptr(ApiError_Http("Unauthorized")));
            } else if (url == "https://api.github.com/repos/mozilla/uniffi-rs/issues/2017") {
                promise.set_value(test_response_data());
            } else {
                promise.set_exception(std::make_exception_ptr(ApiError_Http("Wrong URL: " + url)));
            }
        }).detach();
        return promise.get_future();
    }
};

/// Runs Rust's blocking tasks on a thread of their own.
class CppTaskRunner : public TaskRunner {
public:
    uniffi::Future<void> run_task(const std::shared_ptr<RustTask>& task) override {
        uniffi::Promise<void> promise;
        std::thread([promise, task] {
            task->execute();
            promise.set_value();
        }).detach();
        return promise.get_future();
    }
};

int main() {
    auto client = ApiClient::new_(std::make_shared<CppHttpClient>(), std::make_shared<CppTaskRunner>());
    auto issue = client->get_issue("mozilla", "uniffi-rs", 2017).get();
    CHECK_EQ(issue.title, "Foreign-implemented async traits");
    CHECK_THROWS(client->get_issue("mozilla", "uniffi-rs", 1).get(), ApiError_Http);
    return check::report();
}
