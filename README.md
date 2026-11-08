# uniffi-bindgen-cpp

[![CI](https://github.com/sorinirimies/uniffi-bindgen-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/sorinirimies/uniffi-bindgen-cpp/actions/workflows/ci.yml)
[![Security](https://github.com/sorinirimies/uniffi-bindgen-cpp/actions/workflows/security.yml/badge.svg)](https://github.com/sorinirimies/uniffi-bindgen-cpp/actions/workflows/security.yml)
[![Fuzz](https://github.com/sorinirimies/uniffi-bindgen-cpp/actions/workflows/fuzz.yml/badge.svg)](https://github.com/sorinirimies/uniffi-bindgen-cpp/actions/workflows/fuzz.yml)
[![crates.io](https://img.shields.io/crates/v/uniffi-bindgen-cpp.svg)](https://crates.io/crates/uniffi-bindgen-cpp)

C++17 bindings generator for [UniFFI](https://mozilla.github.io/uniffi-rs/) 0.32.

Point it at a library built with UniFFI and it writes a header and a source file per
component, plus one shared runtime header. The generated code calls the scaffolding
through its C ABI, so you link the Rust library like any other native library.

* **Complete:** passes uniffi's own fixture suite (functions, records, enums, errors,
  objects, traits, callback interfaces, async, custom and external types, renames,
  docstrings) with GCC, Clang and under ASan/UBSan, and is tested on Linux, macOS and
  Windows (MSVC).
* **Async that fits C++:** `uniffi::Future<T>` can be waited on, cancelled, continued
  with `then()`, or `co_await`ed in C++20; continuations run on your event loop if you
  want. Async callback methods are completed with `uniffi::Promise<T>`.
* **Plain C++ types:** structs, `enum class`, `std::variant`, `std::optional`,
  `std::vector`, `std::map`, `std::set`, `std::shared_ptr`, exceptions; `==`, `<`, `std::hash`
  and `operator<<` where Rust exports them.
* **Builds with CMake:** one call builds the crate with cargo, another generates and
  compiles its bindings.

Used to ship [stem-mqtt](https://github.com/sorinirimies/stem-mqtt) (an MQTT broker and
client) to C++; see [examples/stem-mqtt](examples/stem-mqtt).

## Install

```sh
cargo install uniffi-bindgen-cpp
```

or download the archive for your platform from the
[latest release](https://github.com/sorinirimies/uniffi-bindgen-cpp/releases).

| uniffi-bindgen-cpp | UniFFI |
|---|---|
| 0.2 | 0.32 |
| 0.1 | 0.31 |

A UniFFI minor version can change the FFI (0.32 passes `&[u8]` differently), so use the
release that matches the `uniffi` version of the Rust library.

## Use

### With CMake

```sh
uniffi-bindgen-cpp cmake cmake/      # writes cmake/UniffiCpp.cmake
```

```cmake
include(cmake/UniffiCpp.cmake)

# Build the crate as a cdylib with cargo.
uniffi_cargo_library(mylib_rust MANIFEST ${CMAKE_SOURCE_DIR}/rust/Cargo.toml NAME mylib)
# Generate and compile its bindings: a static library that links the Rust one.
uniffi_cpp_bindings(mylib RUST mylib_rust COMPONENTS mylib)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE mylib)
```

`COMPONENTS` names the UniFFI namespaces in the library (one header each). A library built
elsewhere works too: `uniffi_cpp_bindings(mylib LIBRARY path/to/libmylib.so COMPONENTS mylib)`.
See the comment at the top of `UniffiCpp.cmake` for every option.

### By hand

Build your crate as a `cdylib` and generate:

```sh
uniffi-bindgen-cpp generate target/release/libmycrate.so --out-dir cpp/
```

This writes `cpp/mycrate.hpp`, `cpp/mycrate.cpp` and `cpp/uniffi_runtime.hpp`. Compile the
`.cpp` files into your program and link the Rust library:

```sh
c++ -std=c++17 -Icpp main.cpp cpp/mycrate.cpp -Ltarget/release -lmycrate
```

Run it from the crate's directory: library mode finds UDL files through `cargo metadata`.
A UDL file works too: `uniffi-bindgen-cpp generate src/mycrate.udl --out-dir cpp/ --library target/release/libmycrate.so`.

```cpp
#include "mycrate.hpp"

int main() {
    auto counter = mycrate::Counter::new_(10);   // objects are std::shared_ptr
    counter->add(5);

    try {
        mycrate::risky_operation();
    } catch (const mycrate::MyError_NotFound& e) {
        // errors are exceptions, one class per variant
    }

    auto sum = mycrate::slow_add(2, 3);           // uniffi::Future<int32_t>
    return sum.get();
}
```

## Type mapping

| UniFFI | C++ |
|---|---|
| `u8`…`u64`, `i8`…`i64`, `f32`, `f64`, `bool` | `uint8_t`…`int64_t`, `float`, `double`, `bool` |
| `String` | `std::string`; parameters of calls into Rust are `std::string_view`, overridable virtual methods take `const std::string&` (Rust rejects invalid UTF-8 with an error) |
| `Vec<u8>` / bytes | `std::vector<uint8_t>`; lent to Rust without a copy for `&[u8]` arguments |
| `Vec<T>` | `std::vector<T>` |
| `Option<T>` | `std::optional<T>` |
| `HashMap<K, V>` | `std::map<K, V>` |
| `HashSet<T>` | `std::set<T>` |
| `Box<T>` | `uniffi::Box<T>`: a deep-copying pointer, which lets records and enums contain themselves |
| `SystemTime` | `uniffi::Timestamp` (nanosecond `std::chrono::time_point`) |
| `Duration` | `uniffi::Duration` (`std::chrono::nanoseconds`) |
| record | `struct` with the fields in order; fields take their default or are value-initialized |
| enum without data | `enum class` with the Rust discriminants |
| enum with data | `struct` holding a `std::variant` of one struct per variant; `is<V>()`, `get_if<V>()`, `visit(f)` |
| error | exception class `E`, one subclass `E_Variant` per variant; as a value, `std::shared_ptr<E>` |
| object | `class`, held as `std::shared_ptr`; constructors are static functions |
| trait / callback interface | abstract class; implement it in C++ and pass a `std::shared_ptr` |
| `async fn` | returns `uniffi::Future<T>` |
| custom type | alias of its builtin type, or a C++ type of your choice (see below) |

Names are kept as written in Rust. C++ keywords get a trailing `_` (`new` becomes `new_`),
and so does a member that would clash with its class. Rust doc comments become `///`
comments.

### Methods and traits

Methods and constructors of records and enums become members (free functions taking the
value first, for an `enum class`). Exported uniffi traits become:

| Rust | C++ |
|---|---|
| `Display` | `to_string()`, `operator<<`, and `what()` of an error |
| `Debug` | `debug_string()` |
| `Eq` | `==`, `!=` |
| `Ord` | `compare()`, `<`, `>`, `<=`, `>=` |
| `Hash` | `hash()`, and `std::hash` for records and enums |

Records and enums without an exported `Eq` compare field by field (objects inside them
through their own `==` if they have one).

### Errors

Errors are thrown. A variant's fields are public members; flat errors carry `message`.
Rust panics throw `uniffi::PanicError`; everything the bindings throw derives from
`uniffi::Exception` (a `std::runtime_error`). An object or trait object can be an error
too: catch it by its class. A C++ callback may throw the error type its method declares;
any other exception reaches Rust as an unexpected error.

### Async

`uniffi::Future<T>` is what an `async fn` returns:

```cpp
auto future = mqtt::connect(options);
future.get();                                   // wait, like std::future
future.wait_for(std::chrono::seconds(1));
future.cancel();                                // drops the Rust future; get() throws uniffi::CancelledError
mqtt::fetch(id).then([](uniffi::Future<Item> done) { use(done.get()); });   // no blocking
auto item = co_await mqtt::fetch(id);           // C++20
```

Continuations (polling the Rust future, the callbacks given to `then()`, coroutine
resumption) run on one background thread by default. To run them on your own event loop:

```cpp
uniffi::set_executor([](std::function<void()> task) { my_loop.post(std::move(task)); });
```

An async method of a callback interface returns a `uniffi::Future<T>`; complete it with a
`uniffi::Promise<T>`. When Rust drops the call, the promise reports it:

```cpp
uniffi::Future<std::string> fetch(const std::string& url) override {
    uniffi::Promise<std::string> promise;
    promise.on_cancel([request = start(url, promise)] { request->abort(); });
    return promise.get_future();
}
```

`uniffi::Future<T>::ready(value)`, `failed(error)` and `from(std::future)` cover the other
cases.

## Configuration

Settings go in the `[bindings.cpp]` section of the crate's `uniffi.toml`, or in a file passed
with `--config`:

```toml
[bindings.cpp]
namespace = "mqtt"          # default: the UniFFI namespace
omit_checksums = false

[bindings.cpp.custom_types.Timestamp]
type_name = "std::chrono::system_clock::time_point"
includes = ["<chrono>"]
lift = "std::chrono::system_clock::time_point(std::chrono::milliseconds({}))"   # builtin -> C++
lower = "std::chrono::duration_cast<std::chrono::milliseconds>({}.time_since_epoch()).count()"

[bindings.cpp.rename]       # UniFFI's dot notation
Record = "CppRecord"
"Record.field" = "cpp_field"
"Object.method.arg" = "cpp_arg"
```

## Limits

* Types that come from another crate use that crate's UniFFI namespace, not a namespace
  configured for it.
* The runtime header must come from the same release for all bindings in one program.
* A custom type mapped to the same C++ type in two components defines its codec twice.
* `HashSet<T>` is a `std::set<T>`, so `T` needs `<`: numbers, strings, enums without
  data, and records or enums that export `Ord`.
* uniffi does not support `&[u8]` arguments in async functions or in methods of traits
  implemented in C++.

## Development

```sh
just              # list tasks
just check-all    # the quality gate: fmt, clippy, tests, versions, nu tests
just e2e          # generate C++ for the tour fixture, compile and run it
just fixtures     # the same for uniffi's fixture suite (add --sanitize, or --only <lib>)
just cmake        # build and test the CMake examples
just tidy         # clang-tidy on the generated C++ (.clang-tidy; findings fail CI)
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
fixtures/uniffi/  uniffi-rs v0.32.2 fixtures and examples (scripts/vendor_fixtures.nu), each with its C++ test in cpp/ (just fixtures)
examples/         CMake projects: the tour crate, and stem-mqtt's broker and client (just cmake)
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
