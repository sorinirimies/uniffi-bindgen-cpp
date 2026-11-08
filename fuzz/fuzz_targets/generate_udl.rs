#![no_main]
//! Whatever text we are given, parsing it as UDL either fails cleanly or yields
//! a definition the generator renders without panicking, with and without
//! exceptions; a rendered header always has its guard and namespace.

use libfuzzer_sys::fuzz_target;
use uniffi_bindgen_cpp::{Config, generate_from_udl};

fuzz_target!(|data: &[u8]| {
    let Ok(udl) = std::str::from_utf8(data) else { return };
    for exceptions in [true, false] {
        let config = Config { exceptions, ..Config::default() };
        if let Ok(files) = generate_from_udl(udl, "fuzz", &config) {
            assert!(files.header.contains("#pragma once"));
            assert!(files.header.contains("namespace fuzz {"));
            assert!(files.source.contains("#include \"fuzz.hpp\""));
            // Only bindings that throw refuse to build without exceptions.
            assert_eq!(files.header.contains("#if !UNIFFI_CPP_EXCEPTIONS"), exceptions);
        }
    }
});
