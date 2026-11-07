// Port of uniffi-rs v0.31.2 fixtures/benchmarks/benches/bindings/run_benchmarks.py:
// Rust's Criterion measures calls in both directions; the arguments of this
// program are Criterion's (a name filter, --save-baseline, ...).
#include <chrono>

#include "benchmarks.hpp"

using namespace benchmarks;

namespace {

const std::string LARGE_STRING_1(2048, 'a');
const std::string LARGE_STRING_2(1500, 'b');
const TestRecord REC1{-1, 1, 1.5};
const TestRecord REC2{-2, 2, 4.5};
const TestEnum ENUM1{TestEnum::One{-1, 0}};
const TestEnum ENUM2{TestEnum::Two{1.5}};
const std::vector<uint32_t> VEC1{0, 1};
const std::vector<uint32_t> VEC2{2, 4, 6};
const std::map<uint32_t, uint32_t> MAP1{{0, 1}, {1, 2}};
const std::map<uint32_t, uint32_t> MAP2{{2, 4}};

NestedData nested(int which) {
    if (which == 1) {
        return NestedData{{TestRecord{-1, 1, 1.5}},
                          {{"one", "two"}, {"three"}},
                          {{"one", TestEnum{TestEnum::One{-1, 1}}}, {"two", TestEnum{TestEnum::Two{0.5}}}}};
    }
    return NestedData{{TestRecord{-2, 2, 4.5}}, {{"four", "five"}}, {{"two", TestEnum{TestEnum::Two{-0.5}}}}};
}

double sum(const TestEnum& e) {
    if (auto one = e.get_if<TestEnum::One>()) {
        return one->a + static_cast<double>(one->b);
    }
    return e.get_if<TestEnum::Two>()->c;
}

class Callbacks : public TestCallbackInterface {
public:
    Callbacks()
        : interface1_(TestInterface::new_()),
          interface2_(TestInterface::new_()),
          trait1_(make_test_trait_interface()),
          trait2_(make_test_trait_interface()),
          nested1_(nested(1)),
          nested2_(nested(2)) {}

    void call_only() override {}
    double primitives(uint8_t a, int32_t b) override { return static_cast<double>(a) + b; }
    std::string strings(const std::string& a, const std::string& b) override { return a + b; }
    std::string large_strings(const std::string& a, const std::string& b) override { return a + b; }
    TestRecord records(const TestRecord& a, const TestRecord& b) override {
        return TestRecord{a.a + b.a, a.b + b.b, a.c + b.c};
    }
    TestEnum enums(const TestEnum& a, const TestEnum& b) override { return TestEnum{TestEnum::Two{sum(a) + sum(b)}}; }
    std::vector<uint32_t> vecs(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) override {
        auto out = a;
        out.insert(out.end(), b.begin(), b.end());
        return out;
    }
    std::map<uint32_t, uint32_t> hash_maps(const std::map<uint32_t, uint32_t>& a,
                                           const std::map<uint32_t, uint32_t>& b) override {
        auto out = a;
        for (const auto& [key, value] : b) {
            out[key] = value;
        }
        return out;
    }
    std::shared_ptr<TestInterface> interfaces(const std::shared_ptr<TestInterface>& a,
                                              const std::shared_ptr<TestInterface>& b) override {
        return a == b ? a : b;
    }
    std::shared_ptr<TestTraitInterface> trait_interfaces(const std::shared_ptr<TestTraitInterface>& a,
                                                         const std::shared_ptr<TestTraitInterface>& b) override {
        return a == b ? a : b;
    }
    NestedData nested_data(const NestedData& a, const NestedData& b) override {
        NestedData out = a;
        out.a.insert(out.a.end(), b.a.begin(), b.a.end());
        out.b.insert(out.b.end(), b.b.begin(), b.b.end());
        for (const auto& [key, value] : b.c) {
            out.c[key] = value;
        }
        return out;
    }
    uint32_t errors() override { throw TestError_Two(); }

    uint64_t run_test(TestCase test_case, uint64_t count) override {
        auto start = std::chrono::steady_clock::now();
        for (uint64_t i = 0; i < count; ++i) {
            switch (test_case) {
                case TestCase::CallOnly: test_case_call_only(); break;
                case TestCase::Primitives: (void)test_case_primitives(0, -1); break;
                case TestCase::Strings: (void)test_case_strings("a", "b"); break;
                case TestCase::LargeStrings: (void)test_case_large_strings(LARGE_STRING_1, LARGE_STRING_2); break;
                case TestCase::Records: (void)test_case_records(REC1, REC2); break;
                case TestCase::Enums: (void)test_case_enums(ENUM1, ENUM2); break;
                case TestCase::Vecs: (void)test_case_vecs(VEC1, VEC2); break;
                case TestCase::Hashmaps: (void)test_case_hashmaps(MAP1, MAP2); break;
                case TestCase::Interfaces: (void)test_case_interfaces(interface1_, interface2_); break;
                case TestCase::TraitInterfaces: (void)test_case_trait_interfaces(trait1_, trait2_); break;
                case TestCase::NestedData: (void)test_case_nested_data(nested1_, nested2_); break;
                case TestCase::Errors:
                    try {
                        test_case_errors();
                    } catch (const TestError&) {
                    }
                    break;
            }
        }
        auto elapsed = std::chrono::steady_clock::now() - start;
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
    }

private:
    std::shared_ptr<TestInterface> interface1_, interface2_;
    std::shared_ptr<TestTraitInterface> trait1_, trait2_;
    NestedData nested1_, nested2_;
};

}  // namespace

int main() {
    run_benchmarks("cpp", std::make_shared<Callbacks>());
    return 0;
}
