// Fuzzes the decoding of values coming from Rust: whatever the bytes, reading them
// either produces a value or throws uniffi::Exception; it never reads out of
// bounds, overflows, or allocates without bound.
//
// With libFuzzer: clang++ -std=c++17 -fsanitize=fuzzer,address,undefined -I<runtime dir> fuzz_codecs.cpp
// Without it (-DUNIFFI_FUZZ_SMOKE), main() feeds it pseudo-random inputs.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>

#include "uniffi_runtime.hpp"

namespace {

template <class T>
void decode(const uint8_t* data, size_t size) {
    uniffi::Reader reader(data, size);
    try {
        T value = uniffi::Codec<T>::read(reader);
        // What was read must write back to something that reads back the same.
        uniffi::Writer writer;
        uniffi::Codec<T>::write(writer, value);
        uniffi::Reader again(writer.bytes().data(), writer.bytes().size());
        T copy = uniffi::Codec<T>::read(again);
        if (!uniffi::equal(value, copy) && value == value) {
            std::abort();
        }
    } catch (const uniffi::Exception&) {
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    // The first byte picks the type, the rest is the buffer.
    const uint8_t* rest = data + 1;
    size_t n = size - 1;
    switch (data[0] % 8) {
        case 0: decode<std::string>(rest, n); break;
        case 1: decode<std::vector<uint8_t>>(rest, n); break;
        case 2: decode<std::vector<std::optional<std::string>>>(rest, n); break;
        case 3: decode<std::map<std::string, std::vector<int64_t>>>(rest, n); break;
        case 4: decode<uniffi::Timestamp>(rest, n); break;
        case 5: decode<uniffi::Duration>(rest, n); break;
        case 6: decode<std::optional<std::map<int32_t, std::optional<std::vector<uint8_t>>>>>(rest, n); break;
        default: decode<std::vector<std::vector<std::vector<bool>>>>(rest, n); break;
    }
    return 0;
}

#ifdef UNIFFI_FUZZ_SMOKE
int main(int argc, char** argv) {
    unsigned long runs = argc > 1 ? std::strtoul(argv[1], nullptr, 10) : 200000;
    std::mt19937_64 rng(42);
    std::vector<uint8_t> input;
    for (unsigned long i = 0; i < runs; ++i) {
        input.resize(rng() % 64);
        for (auto& byte : input) {
            // Mostly small values, so lengths and tags are often plausible.
            byte = static_cast<uint8_t>(rng() % 4 == 0 ? rng() : rng() % 4);
        }
        LLVMFuzzerTestOneInput(input.data(), input.size());
    }
    std::printf("%lu inputs, no crash\n", runs);
    return 0;
}
#endif
