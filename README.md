# uniffi-bindgen-cpp

[![CI](https://github.com/sorinirimies/uniffi-bindgen-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/sorinirimies/uniffi-bindgen-cpp/actions/workflows/ci.yml)
[![Security](https://github.com/sorinirimies/uniffi-bindgen-cpp/actions/workflows/security.yml/badge.svg)](https://github.com/sorinirimies/uniffi-bindgen-cpp/actions/workflows/security.yml)
[![Fuzz](https://github.com/sorinirimies/uniffi-bindgen-cpp/actions/workflows/fuzz.yml/badge.svg)](https://github.com/sorinirimies/uniffi-bindgen-cpp/actions/workflows/fuzz.yml)
[![crates.io](https://img.shields.io/crates/v/uniffi-bindgen-cpp.svg)](https://crates.io/crates/uniffi-bindgen-cpp)

C++17 bindings generator for [UniFFI](https://mozilla.github.io/uniffi-rs/) 0.31.

Point it at a library built with UniFFI and it writes a header and a source file per
component, plus one shared runtime header. The generated code calls the scaffolding
through its C ABI, so you link the Rust library like any other native library.
Rust `async fn`s become `std::future`s, Rust callbacks become abstract classes.

Used to ship [stem-mqtt](https://github.com/sorinirimies/stem-mqtt) (an MQTT client and
broker) to C++.

## Install

```sh
cargo install uniffi-bindgen-cpp
```

or download the archive for your platform from the
[latest release](https://github.com/sorinirimies/uniffi-bindgen-cpp/releases).

## Use

Build your crate as a `cdylib` (or `staticlib`) and generate:

```sh
uniffi-bindgen-cpp generate target/release/libmycrate.so --out-dir cpp/
```

This writes `cpp/mycrate.hpp`, `cpp/mycrate.cpp` and `cpp/uniffi_runtime.hpp`. Compile the
`.cpp` files into your program and link the Rust library:

```sh
c++ -std=c++17 -Icpp main.cpp cpp/mycrate.cpp -Ltarget/release -lmycrate
```

A UDL file works too: `uniffi-bindgen-cpp generate src/mycrate.udl --out-dir cpp/ --library target/release/libmycrate.so`.

```cpp
#include "mycrate.hpp"

int main() {
    auto counter = mycrate::Counter::new_(10);
    counter->add(5);

    try {
        mycrate::risky_operation();
    } catch (const mycrate::MyError_NotFound&) {
        // errors are exceptions, one class per variant
    }

    auto sum = mycrate::slow_add(2, 3);   // std::future<int32_t>
    return sum.get();
}
```

## Type mapping

| UniFFI | C++ |
|---|---|
| `u8`…`u64`, `i8`…`i64`, `f32`, `f64`, `bool` | `uint8_t`…`int64_t`, `float`, `double`, `bool` |
| `String` | `std::string` |
| `Vec<u8>` / bytes | `std::vector<uint8_t>` |
| `Vec<T>` | `std::vector<T>` |
| `Option<T>` | `std::optional<T>` |
| `HashMap<K, V>` | `std::map<K, V>` |
| `SystemTime` | `uniffi::Timestamp` (nanosecond `std::chrono::time_point`) |
| `Duration` | `uniffi::Duration` (`std::chrono::nanoseconds`) |
| record | `struct` with the fields, in order; defaults become default member initializers |
| enum without data | `enum class` |
| enum with data | `struct` holding a `std::variant`; `is<V>()`, `get_if<V>()`, `visit(f)` |
| error enum | an exception class `E` and one subclass `E_Variant` per variant |
| object | `class`, held as `std::shared_ptr`; constructors are static functions |
| trait / callback interface | abstract class; implement it in C++ and pass a `std::shared_ptr` |
| `async fn` | returns `std::future<T>` |
| custom type | alias of its builtin type, or a C++ type of your choice (see below) |

Names are kept as written in Rust; C++ keywords get a trailing `_` (`new` becomes `new_`).
Rust doc comments become `///` comments.

### Errors

Errors are thrown. A variant's fields are public members. Panics in Rust throw
`uniffi::PanicError`; everything the bindings throw derives from `uniffi::Exception`.
A C++ callback implementation may throw the error type its method declares; any other
exception reaches Rust as an unexpected error.

### Async

A Rust future is polled on a background thread, so `std::future::get()` blocks the calling
thread only. Dropping the `std::future` does not cancel the Rust future. An async method of
a callback interface returns `std::future<T>` in C++.

## Configuration

Settings go in the `[bindings.cpp]` section of the crate's `uniffi.toml`, or in a file passed
with `--config`:

```toml
[bindings.cpp]
namespace = "mqtt"          # default: the UniFFI namespace
omit_checksums = false

[bindings.cpp.custom_types.Url]
type_name = "std::string"   # a C++ type
includes = ["<string>"]
lift = "{}"                 # builtin value -> C++ value
lower = "{}"                # C++ value -> builtin value
```

## Not supported yet

* Methods and constructors on records and enums.
* Objects used as error types.
* Per-component namespace overrides for types that come from another crate (the other
  component's UniFFI namespace is used).

## Other C++ generators

[NordSecurity/uniffi-bindgen-cpp](https://github.com/NordSecurity/uniffi-bindgen-cpp) is an
older project with the same goal. It requires C++20, does not support async functions, and
can also generate C++ scaffolding. This project targets C++17, supports `async`, and is
tested end to end against real UniFFI crates (`fixtures/tour`, stem-mqtt).

## Development

```sh
just              # list tasks
just check-all    # the quality gate: fmt, clippy, tests, versions, nu tests
just e2e          # generate C++ for the tour fixture, compile and run it
just fuzz-all     # every cargo-fuzz target (nightly)
just audit deny   # supply chain
just update-deps  # cargo update (+ fuzz/) → gate → commit → push
just release 0.2.0       # bump, changelog, tag, push → Release workflow
just release-all 0.2.0   # … to GitHub and Gitea
```

Layout:

```
crates/uniffi-bindgen-cpp/   the crate: codegen/ (the generator), runtime/ (the C++ runtime header), cli, config; tests/
fixtures/tour/    a UniFFI crate that uses every feature, with the C++ test that runs against it (just e2e)
fuzz/             cargo-fuzz targets (nightly), one per parser/decoder
scripts/          all tooling, Nushell only (justfile recipes run in nu too):
                  version, bump, release, remotes, CI helpers, quality gate —
                  each with tests in scripts/tests/
```

CI runs on GitHub **and** Gitea (a nu test keeps the two in sync): quality gate,
tests on macOS and Windows, static musl build, MSRV, and three nightlies — lockfile
update + gate + push (GitHub 02:00, Gitea 02:30 UTC), `cargo audit` + `cargo deny`
(03:00), fuzzing every target (04:00). Dependabot keeps the GitHub actions current.
Commits follow [Conventional Commits](https://www.conventionalcommits.org);
`CHANGELOG.md` is generated by [git-cliff](https://git-cliff.org) — don't edit it by hand.

## License

MIT
