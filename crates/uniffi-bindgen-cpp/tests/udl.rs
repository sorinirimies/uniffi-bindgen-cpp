//! Generate from UDL and check the shape of the output. The compile check at
//! the end uses the system C++ compiler and is skipped when there is none.

use std::process::Command;

use uniffi_bindgen_cpp::{
    Config, GeneratedFiles, RUNTIME_HEADER, RUNTIME_HEADER_NAME, generate_from_udl,
};

const UDL: &str = r#"
namespace sample {
    u32 add(u32 a, u32 b);
    [Throws=Oops] string risky(string s);
    Shape shape_of(Point p, optional double radius = 2.5);
    [Async] string fetch(string key);
    sequence<string> names();
    record<string, u32> counts();
    bytes reverse(bytes data);
};

dictionary Holder {
    Thing thing;
};

[Error]
enum Oops { "Bad", "Worse" };

[Error]
interface Detailed {
    Code(i32 code);
    Empty();
};

enum Color { "Red", "Green" };

dictionary Point {
    double x;
    double y = 1.5;
    string? label = null;
};

[Enum]
interface Shape {
    Circle(Point center, double radius);
    Empty();
};

interface Thing {
    constructor(string name);
    [Name=other] constructor(u32 n);
    string name();
    [Throws=Oops] void check();
    [Async] u32 later();
};

callback interface Sink {
    void push(string s);
    [Throws=Oops] u32 count();
};
"#;

fn generate() -> GeneratedFiles {
    generate_from_udl(UDL, "sample", &Config::default()).expect("generates")
}

#[test]
fn header_declares_every_kind_of_definition() {
    let h = generate().header;
    for expected in [
        "#pragma once",
        "namespace sample {",
        "uint32_t add(uint32_t a, uint32_t b);",
        "std::string risky(const std::string& s);",
        "uniffi::Future<std::string> fetch(const std::string& key);",
        "std::vector<std::string> names();",
        "std::map<std::string, uint32_t> counts();",
        "enum class Color : int32_t {",
        "struct Point {",
        "double y = 1.5;",
        "std::optional<std::string> label = std::nullopt;",
        "struct Shape {",
        "using Variant = std::variant<Circle, Empty>;",
        "class Oops : public uniffi::Exception {",
        "class Oops_Bad final : public Oops {",
        "class Detailed_Code final : public Detailed {",
        "class Thing {",
        "static std::shared_ptr<::sample::Thing> new_(const std::string& name);",
        "static std::shared_ptr<::sample::Thing> other(uint32_t n);",
        "uniffi::Future<uint32_t> later() const;",
        "class Sink {",
        "virtual void push(const std::string& s) = 0;",
        "struct Codec<::sample::Point>",
    ] {
        assert!(h.contains(expected), "header is missing `{expected}`:\n{h}");
    }
}

#[test]
fn header_keeps_trailing_argument_defaults() {
    let h = generate().header;
    assert!(
        h.contains("::sample::Shape shape_of(const ::sample::Point& p, double radius = 2.5);"),
        "{h}"
    );
}

#[test]
fn source_calls_the_scaffolding_and_checks_status() {
    let s = generate().source;
    for expected in [
        "#include \"sample.hpp\"",
        "namespace uniffi_ffi_sample {",
        "uniffi_detail::ensure_init();",
        "uniffi_detail::check_status(uniffi_status, &uniffi_detail::throw_buf_Oops);",
        "uniffi::detail::start_future<std::string>(",
        "Thing::~Thing()",
        "uint64_t Thing::uniffi_clone_handle() const",
        "void init_Sink()",
    ] {
        assert!(s.contains(expected), "source is missing `{expected}`:\n{s}");
    }
}

#[test]
fn config_namespace_overrides_the_udl_namespace() {
    let config = Config {
        namespace: Some("mine".into()),
        ..Config::default()
    };
    let files = generate_from_udl(UDL, "sample", &config).expect("generates");
    assert!(files.header.contains("namespace mine {"));
    assert!(files.source.contains("namespace mine {"));
    assert_eq!(files.header_name, "sample.hpp");
}

#[test]
fn config_omit_checksums_drops_the_check() {
    let with = generate().source;
    let without = generate_from_udl(
        UDL,
        "sample",
        &Config {
            omit_checksums: true,
            ..Config::default()
        },
    )
    .expect("generates")
    .source;
    assert!(with.contains("API checksum mismatch"));
    assert!(!without.contains("API checksum mismatch"));
}

/// Compiles `files` with the system C++ compiler (warnings are errors). Returns
/// `false` when there is no compiler, so the caller can skip.
fn compiles(files: &GeneratedFiles) -> bool {
    let Some(compiler) = ["c++", "clang++", "g++"].into_iter().find(|c| {
        Command::new(c)
            .arg("--version")
            .output()
            .is_ok_and(|o| o.status.success())
    }) else {
        eprintln!("no C++ compiler found, skipping");
        return false;
    };
    let dir = tempfile::tempdir().expect("temp dir");
    let path = |name: &str| dir.path().join(name);
    std::fs::write(path(RUNTIME_HEADER_NAME), RUNTIME_HEADER).expect("write runtime");
    std::fs::write(path(&files.header_name), &files.header).expect("write header");
    std::fs::write(path(&files.source_name), &files.source).expect("write source");
    let out = Command::new(compiler)
        .args(["-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsyntax-only"])
        .arg(path(&files.source_name))
        .output()
        .expect("run the compiler");
    assert!(
        out.status.success(),
        "{}",
        String::from_utf8_lossy(&out.stderr)
    );
    true
}

#[test]
fn generated_code_compiles() {
    compiles(&generate());
}

// Regression: with no functions the status helper was unused, and `-Werror`
// builds of the generated code failed with `-Wunused-function`.
#[test]
fn empty_namespace_generates_compilable_code() {
    let files = generate_from_udl("namespace empty {};", "empty", &Config::default()).unwrap();
    assert!(files.header.contains("namespace empty {"));
    compiles(&files);
}

#[test]
fn invalid_udl_is_an_error() {
    assert!(
        generate_from_udl(
            "namespace broken { u32 f(; };",
            "broken",
            &Config::default()
        )
        .is_err()
    );
}

// Regression: bytes were once lowered as a raw buffer without the length prefix
// Rust reads, so every `bytes` argument arrived garbled.
#[test]
fn bytes_go_through_the_length_prefixed_codec() {
    let files = generate();
    assert!(
        files
            .source
            .contains("uniffi_detail::lower_buf<std::vector<uint8_t>>(data)")
    );
    assert!(
        files
            .source
            .contains("uniffi_detail::lift_buf<std::vector<uint8_t>>(uniffi_result)")
    );
    assert!(!files.source.contains("lower_bytes") && !files.source.contains("lift_bytes"));
}

// Regression: a record holding an object used `Codec<std::shared_ptr<Obj>>` before
// it was defined, which does not compile. Value codecs are now defined out of line,
// after every codec class.
#[test]
fn value_codecs_are_defined_after_every_codec_class() {
    let h = generate().header;
    let object = h
        .find("struct Codec<std::shared_ptr<::sample::Thing>> {")
        .expect("object codec");
    let record_class = h
        .find("struct Codec<::sample::Holder> {")
        .expect("record codec class");
    let record_read = h
        .find("inline ::sample::Holder Codec<::sample::Holder>::read(Reader& reader) {")
        .expect("record codec definition");
    assert!(object < record_read && record_class < record_read);
}

// Regression: callback vtable types were qualified twice (`ffi::ffi::`).
#[test]
fn ffi_names_are_qualified_once() {
    assert!(!generate().source.contains("ffi::ffi::"));
}
