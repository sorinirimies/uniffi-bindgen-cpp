// Port of uniffi-rs v0.31.2 examples/callbacks/tests/bindings/test_callbacks.py
#include <stdexcept>

#include "callbacks.hpp"
#include "check.hpp"

using namespace callbacks;

class CallAnswererImpl : public CallAnswerer {
public:
    explicit CallAnswererImpl(std::string mode) : mode_(std::move(mode)) {}
    std::string answer() override {
        if (mode_ == "ready") {
            return "Bonjour";
        }
        if (mode_ == "busy") {
            throw TelephoneError_Busy("busy");
        }
        throw std::runtime_error("Testing an unexpected error");
    }

private:
    std::string mode_;
};

class DiscountSim : public SimCard {
public:
    std::string name() override { return "c++"; }
};

template <class Phone>
void run() {
    auto telephone = Phone::new_();
    auto sim = get_sim_cards()[0];
    CHECK_EQ(telephone->call(sim, std::make_shared<CallAnswererImpl>("ready")), "Bonjour");
    CHECK_THROWS(telephone->call(sim, std::make_shared<CallAnswererImpl>("busy")), TelephoneError_Busy);
    CHECK_THROWS(telephone->call(sim, std::make_shared<CallAnswererImpl>("something-else")),
                 TelephoneError_InternalTelephoneError);
    CHECK_EQ(telephone->call(std::make_shared<DiscountSim>(), std::make_shared<CallAnswererImpl>("ready")),
             "c++ est bon march\xC3\xA9");
}

int main() {
    run<Telephone>();
    run<FancyTelephone>();
    return check::report();
}
