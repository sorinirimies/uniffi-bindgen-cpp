//! Generate from UDL and check the shape of the output. The compile check at
//! the end uses the system C++ compiler and is skipped when there is none.

use std::process::Command;

use uniffi_bindgen_cpp::{Config, GeneratedFiles, generate_from_udl};

const UDL: &str = r#"
namespace sample {
    u32 add(u32 a, u32 b);
    [Throws=Oops] string risky(string s);
    Shape shape_of(Point p, optional double radius = 2.5);
    [Async] string fetch(string key);
    sequence<string> names();
    record<string, u32> counts();
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
fn header_declares_the_api() {
    let h = generate().header;
    for expected in [
        "#pragma once",
        "namespace sample {",
        "uint32_t add(uint32_t a, uint32_t b);",
        "std::string risky(const std::string& s);",
        "std::future<std::string> fetch(const std::string& key);",
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
        "static std::shared_ptr<Thing> new_(const std::string& name);",
        "static std::shared_ptr<Thing> other(uint32_t n);",
        "std::future<uint32_t> later() const;",
        "class Sink {",
        "virtual void push(const std::string& s) = 0;",
        "struct Codec<sample::Point>",
    ] {
        assert!(h.contains(expected), "header is missing `{expected}`:\n{h}");
    }
}

#[test]
fn trailing_defaults_only() {
    let h = generate().header;
    assert!(h.contains("shape_of(const Point& p, double radius = 2.5);") || h.contains("double radius = static_cast<double>"), "{h}");
}

#[test]
fn source_calls_the_scaffolding() {
    let s = generate().source;
    for expected in [
        "#include \"sample.hpp\"",
        "namespace uniffi_ffi_sample {",
        "uniffi_detail::ensure_init();",
        "uniffi_detail::check_status(status, &uniffi_detail::throw_buf_Oops);",
        "uniffi::detail::start_future<std::string>(",
        "Thing::~Thing()",
        "uint64_t Thing::uniffi_clone_handle() const",
        "void init_Sink()",
    ] {
        assert!(s.contains(expected), "source is missing `{expected}`:\n{s}");
    }
}

#[test]
fn namespace_can_be_overridden() {
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
fn checksums_can_be_omitted() {
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

#[test]
fn generated_code_compiles() {
    let Some(compiler) = ["c++", "clang++", "g++"].into_iter().find(|c| {
        Command::new(c)
            .arg("--version")
            .output()
            .is_ok_and(|o| o.status.success())
    }) else {
        eprintln!("no C++ compiler found, skipping");
        return;
    };
    let dir = std::env::temp_dir().join(format!("uniffi-bindgen-cpp-compile-{}", std::process::id()));
    std::fs::create_dir_all(&dir).expect("temp dir");
    let files = generate();
    std::fs::write(dir.join("uniffi_runtime.hpp"), uniffi_bindgen_cpp::RUNTIME_HEADER).expect("write");
    std::fs::write(dir.join(&files.header_name), &files.header).expect("write");
    std::fs::write(dir.join(&files.source_name), &files.source).expect("write");
    let out = Command::new(compiler)
        .args(["-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsyntax-only"])
        .arg(dir.join(&files.source_name))
        .output()
        .expect("run the compiler");
    let _ = std::fs::remove_dir_all(&dir);
    assert!(out.status.success(), "{}", String::from_utf8_lossy(&out.stderr));
}
