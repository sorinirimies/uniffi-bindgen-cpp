// An MQTT broker and client, both written in Rust (stem-mqtt), driven from C++:
// start the broker, connect, subscribe, publish, receive through a listener and
// through the message queue, and see a typed error for a refused connection.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

#include "mqtt_broker.hpp"
#include "mqtt_client.hpp"

using namespace std::chrono_literals;

static int failures = 0;
#define EXPECT(cond)                                                              \
    do {                                                                          \
        if (!(cond)) {                                                            \
            ++failures;                                                           \
            std::fprintf(stderr, "%s:%d: expected %s\n", __FILE__, __LINE__, #cond); \
        }                                                                         \
    } while (0)

/// Collects what the client receives; called on a Rust thread.
class Collector : public mqtt_client::MqttMessageListener {
public:
    void on_message(const mqtt_client::MqttMessage& message) override {
        std::lock_guard<std::mutex> lock(mutex);
        received.push_back(message.topic + "=" + std::string(message.payload.begin(), message.payload.end()));
    }
    void on_disconnected(const std::string&) override { disconnected = true; }

    std::mutex mutex;
    std::vector<std::string> received;
    std::atomic<bool> disconnected{false};
};

/// Records which clients connect to the broker.
class BrokerEvents : public mqtt_broker::MqttBrokerEventListener {
public:
    void on_client_connected(const std::string& client_id) override {
        std::lock_guard<std::mutex> lock(mutex);
        connected.push_back(client_id);
    }
    void on_client_disconnected(const std::string&, const std::string&) override {}
    void on_message_published(const std::string&, const std::string&, mqtt_broker::QoS) override {}

    std::mutex mutex;
    std::vector<std::string> connected;
};

int main() {
    mqtt_broker::MqttBrokerConfig config;
    config.bind_address = "127.0.0.1";
    config.port = 0;  // any free port
    config.allow_anonymous = true;
    config.max_clients = 100;
    config.max_qos = mqtt_broker::QoS::ExactlyOnce;
    config.max_retained_messages = 100;
    config.max_queued_per_client = 100;
    config.redelivery_interval_secs = 10;
    auto broker = mqtt_broker::MqttBroker::new_(config);
    auto events = std::make_shared<BrokerEvents>();
    broker->set_event_listener(events);
    broker->start().get();
    auto port = broker->bound_port();
    EXPECT(broker->is_running() && port.has_value());
    std::printf("broker listening on 127.0.0.1:%u\n", port.value_or(0));

    mqtt_client::ConnectOptions options;
    options.host = "127.0.0.1";
    options.port = port.value_or(0);
    options.client_id = "cpp-client";
    options.version = mqtt_client::MqttVersion::V5;
    options.clean_start = true;
    options.keep_alive_secs = 30;
    options.connect_timeout_secs = 5;
    options.operation_timeout_secs = 5;
    options.reconnect_backoff_secs = 1;
    options.reconnect_max_backoff_secs = 5;
    auto client = mqtt_client::MqttClient::new_(options);
    auto collector = std::make_shared<Collector>();
    client->set_message_listener(collector);
    client->enable_message_queue(16);

    EXPECT(client->connect().get().reason_code == 0);
    (void)client->subscribe("demo/#", mqtt_client::QoS::AtLeastOnce).get();
    const std::string text = "hello from c++";
    client->publish("demo/greeting", std::vector<uint8_t>(text.begin(), text.end()), mqtt_client::QoS::AtLeastOnce,
                    false)
        .get();

    auto pulled = client->next_message(3000).get();
    EXPECT(pulled.has_value() && pulled->topic == "demo/greeting");
    for (int i = 0; i < 100 && collector->received.empty(); ++i) {
        std::this_thread::sleep_for(20ms);
    }
    {
        std::lock_guard<std::mutex> lock(collector->mutex);
        EXPECT(collector->received == std::vector<std::string>{"demo/greeting=hello from c++"});
    }
    {
        std::lock_guard<std::mutex> lock(events->mutex);
        EXPECT(events->connected == std::vector<std::string>{"cpp-client"});
    }

    // Nothing listens on port 1: the Rust error arrives as a C++ exception.
    auto refused_options = options;
    refused_options.port = 1;
    try {
        (void)mqtt_client::MqttClient::new_(refused_options)->connect().get();
        EXPECT(!"connect to port 1 succeeded");
    } catch (const mqtt_client::MqttError& error) {
        std::printf("refused as expected: %s\n", error.what());
    }

    client->disconnect().get();
    broker->stop().get();
    EXPECT(!broker->is_running());
    std::printf("%s\n", failures == 0 ? "ok" : "FAILED");
    return failures == 0 ? 0 : 1;
}
