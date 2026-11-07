// Port of uniffi-rs v0.31.2 examples/traits/tests/bindings/test_traits.py
#include "check.hpp"
#include "traits.hpp"

using namespace traits;

class CppButton : public Button {
public:
    std::string name() override { return "CppButton"; }
};

int main() {
    for (const auto& button : get_buttons()) {
        auto name = button->name();
        CHECK(name == "go" || name == "stop");
        CHECK_EQ(press(button)->name(), name);
    }
    CHECK_EQ(press(std::make_shared<CppButton>())->name(), "CppButton");
    return check::report();
}
