# Changelog

All notable changes to this project are documented here. It is generated from
[Conventional Commits](https://www.conventionalcommits.org) by
[git-cliff](https://git-cliff.org) — do not edit it by hand.

## [0.2.0](https://github.com/sorinirimies/uniffi-bindgen-cpp/compare/v0.1.1...v0.2.0) — 2026-11-07

### ⚠️ Breaking changes

- Target UniFFI 0.32 ([`7beb89c`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/7beb89c1dd14c74e0aa3d0bee6a4eb37e42722b0))
- Uniffi::Future with cancellation, continuations, executors and co_await ([`3808bc0`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/3808bc068c61bd75f80a27979fb19337c48c3adb))

### ✨ Features

- Target UniFFI 0.32 ([`7beb89c`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/7beb89c1dd14c74e0aa3d0bee6a4eb37e42722b0))
- C++ runtime fuzzing, snapshot tests, benchmarks; harden decoding ([`e866184`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/e8661841cf55ac82fea372619b7ddd13d5e97af9))
- CMake module, CMake examples (tour, stem-mqtt) and CI for GCC, Clang and MSVC ([`272636a`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/272636a47f87161f2515439a2fb4fcf3300f94c8))
- Ext-types, coverall and async-api-client pass; imported components initialize first ([`b816eaf`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/b816eafc4a1919ac55d7bb7008da2e74201130ad))
- Uniffi::Future with cancellation, continuations, executors and co_await ([`3808bc0`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/3808bc068c61bd75f80a27979fb19337c48c3adb))
- Rename table, and ports of the trait-methods, proc-macro, type-limits, time, docstring, rename and keywords fixtures ([`f303e15`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/f303e157a5d3a26449d4f83f2826c8ae37dba144))
- Errors from other crates, trait objects as errors, Display in what() ([`9b7fe86`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/9b7fe86dad5d32b00e490da58fcd753e4a829946))
- Methods and traits on records and enums, objects as errors ([`f4565a5`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/f4565a51c2dae9333b3ba3bfead40bf9aad56a0c))
- Pass uniffi's fixture suite at compile level ([`1799e39`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/1799e395341690dc6cb5ff4c913f3223359a2687))

### 🐛 Bug fixes

- GCC builds of thrown errors; docs for the async API, CMake and traits ([`7707fcb`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/7707fcba3b362e3cebe4f86d902c220a74b2e675))

### 🧪 Tests

- Futures concurrency check with a margin for slow runners ([`473bdbd`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/473bdbd66c5fe55525e222a33acd9b31e666147f))

### 🔧 Chores

- Stop tracking the tour fixture's build output ([`7b1f78b`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/7b1f78bd226bcb9a84b9e37de953c7958b149fb7))

## [0.1.1](https://github.com/sorinirimies/uniffi-bindgen-cpp/compare/v0.1.0...v0.1.1) — 2026-11-07

### 🐛 Bug fixes

- Generated code compiles with -Werror when a component has no functions ([`6850764`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/68507643b9e60864f4d90596032239fb937cfeb9))

## [0.1.0](https://github.com/sorinirimies/uniffi-bindgen-cpp/releases/tag/v0.1.0) — 2026-11-07

### ✨ Features

- C++ end-to-end test, docs and CI job ([`fc099a7`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/fc099a75e4e67b8c5a0beea9fe18b037c82a88df))
- C++ bindings generator for UniFFI 0.31 ([`480a2d6`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/480a2d65d4207833205d6ac0abfbba5895ff1563))
- Initial project scaffold ([`5d1399b`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/5d1399b209914675a4ab91693f228c63243bf497))

### 📚 Documentation

- Drop the comparison with other C++ generators ([`ec6b831`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/ec6b831bb2cbdacbb25e8b11cf815fa405270f61))

### 📦 Dependencies

- Nightly cargo update 2026-11-07 ([`3b7de55`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/3b7de559d166699541b819181210ae229477d954))
- Ignore the unmaintained basic-toml advisory ([`2f0382d`](https://github.com/sorinirimies/uniffi-bindgen-cpp/commit/2f0382ddcb451d80628b24271950ed94864366b1))


