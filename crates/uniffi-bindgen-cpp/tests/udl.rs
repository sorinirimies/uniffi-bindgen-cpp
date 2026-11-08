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
        "std::string risky(std::string_view s);",
        "uniffi::Future<std::string> fetch(std::string_view key);",
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
        "static std::shared_ptr<::sample::Thing> new_(std::string_view name);",
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

/// The first C++ compiler that runs, if any.
fn cxx_compiler() -> Option<&'static str> {
    ["c++", "clang++", "g++"].into_iter().find(|c| {
        Command::new(c)
            .arg("--version")
            .output()
            .is_ok_and(|o| o.status.success())
    })
}

/// Compiles `files` with the system C++ compiler (warnings are errors) and `extra`
/// flags. `None` when there is no compiler, so the caller can skip.
fn compile(files: &GeneratedFiles, extra: &[&str]) -> Option<std::process::Output> {
    let Some(compiler) = cxx_compiler() else {
        eprintln!("no C++ compiler found, skipping");
        return None;
    };
    let dir = tempfile::tempdir().expect("temp dir");
    let path = |name: &str| dir.path().join(name);
    std::fs::write(path(RUNTIME_HEADER_NAME), RUNTIME_HEADER).expect("write runtime");
    std::fs::write(path(&files.header_name), &files.header).expect("write header");
    std::fs::write(path(&files.source_name), &files.source).expect("write source");
    Some(
        Command::new(compiler)
            .args(["-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsyntax-only"])
            .args(extra)
            .arg(path(&files.source_name))
            .output()
            .expect("run the compiler"),
    )
}

/// `compile` that must succeed. Returns `false` when there is no compiler.
fn compiles(files: &GeneratedFiles) -> bool {
    compiles_with(files, &[])
}

fn compiles_with(files: &GeneratedFiles, extra: &[&str]) -> bool {
    let Some(out) = compile(files, extra) else {
        return false;
    };
    assert!(
        out.status.success(),
        "{}",
        String::from_utf8_lossy(&out.stderr)
    );
    true
}

fn generate_without_exceptions() -> GeneratedFiles {
    let config = Config {
        exceptions: false,
        ..Config::default()
    };
    generate_from_udl(UDL, "sample", &config).expect("generates")
}

#[test]
fn without_exceptions_rust_errors_come_back_in_a_result() {
    let files = generate_without_exceptions();
    let h = &files.header;
    for expected in [
        "uniffi::Result<std::string, std::shared_ptr<::sample::Oops>> risky(std::string_view s);",
        "uniffi::Result<void, std::shared_ptr<::sample::Oops>> check() const;",
        // A C++ implementation returns the error instead of throwing it.
        "virtual uniffi::Result<uint32_t, std::shared_ptr<::sample::Oops>> count() = 0;",
        "std::shared_ptr<::sample::Oops> lift_error_Oops(uniffi::RustBuffer buffer);",
        "std::shared_ptr<::sample::Detailed> lift_error_Detailed(uniffi::RustBuffer buffer);",
        // Calls without a Rust error keep their plain types.
        "uint32_t add(uint32_t a, uint32_t b);",
        "uniffi::Future<std::string> fetch(std::string_view key);",
    ] {
        assert!(h.contains(expected), "missing `{expected}`");
    }
    for text in [h, &files.source] {
        assert!(!text.contains("throw "), "a throw is left");
        assert!(!text.contains("try {"), "a try block is left");
        assert!(!text.contains("uniffi_throw"));
    }
    assert!(!h.contains("#error"));
    compiles_with(&files, &["-fno-exceptions"]);
    // The same bindings also build where exceptions are on.
    compiles(&files);
}

#[test]
fn throwing_bindings_refuse_to_build_without_exceptions() {
    let files = generate();
    assert!(files.header.contains("#if !UNIFFI_CPP_EXCEPTIONS"));
    let Some(out) = compile(&files, &["-fno-exceptions"]) else {
        return;
    };
    assert!(!out.status.success());
    let stderr = String::from_utf8_lossy(&out.stderr);
    assert!(stderr.contains("exceptions = false"), "{stderr}");
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

// uniffi 0.32 passes `[ByRef] bytes` (`&[u8]`) as `ForeignBytes`: lent, not copied.
#[test]
fn borrowed_bytes_are_lent_without_a_copy() {
    let udl = "namespace lend { u32 checksum([ByRef] bytes data); };";
    let files = generate_from_udl(udl, "lend", &Config::default()).expect("generate");
    assert!(files.source.contains("uniffi::borrow_bytes(data)"));
    assert!(
        files
            .header
            .contains("uint32_t checksum(const std::vector<uint8_t>& data);")
    );
    assert!(compiles(&files), "generated code does not compile");
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

// Regression: two sets of bindings each carry a copy of the runtime header, and
// `#pragma once` (which goes by path) let both in: "redefinition of 'RustBuffer'".
#[test]
fn runtime_copies_in_two_directories_are_one() {
    let Some(compiler) = cxx_compiler() else {
        eprintln!("no C++ compiler found, skipping");
        return;
    };
    let dir = tempfile::tempdir().unwrap();
    for sub in ["a", "b"] {
        std::fs::create_dir(dir.path().join(sub)).unwrap();
        std::fs::write(
            dir.path().join(sub).join(RUNTIME_HEADER_NAME),
            RUNTIME_HEADER,
        )
        .unwrap();
    }
    let main = dir.path().join("main.cpp");
    std::fs::write(
        &main,
        "#include \"a/uniffi_runtime.hpp\"\n#include \"b/uniffi_runtime.hpp\"\nint main() { return 0; }\n",
    )
    .unwrap();
    let out = Command::new(compiler)
        .args(["-std=c++17", "-fsyntax-only"])
        .arg(&main)
        .output()
        .expect("run the compiler");
    assert!(
        out.status.success(),
        "{}",
        String::from_utf8_lossy(&out.stderr)
    );
}

// The whole generated output, so that every change to it shows up in review.
// Update with `cargo insta review` (or INSTA_UPDATE=always cargo test).
#[test]
fn generated_header_snapshot() {
    insta::assert_snapshot!("sample_header", generate().header);
}

#[test]
fn generated_source_snapshot() {
    insta::assert_snapshot!("sample_source", generate().source);
}

#[test]
fn string_parameters_of_calls_into_rust_are_views() {
    let udl = r#"
namespace views { string echo(string s); void use_sink(Sink sink); };
interface Thing { constructor(string name); string rename(string to); };
callback interface Sink { string push(string s); };
[Trait, WithForeign] interface Shape { string label(string prefix); };
"#;
    let files = generate_from_udl(udl, "views", &Config::default()).expect("generates");
    let h = &files.header;
    assert!(h.contains("std::string echo(std::string_view s);"));
    assert!(h.contains("std::string_view name"), "constructor");
    assert!(h.contains("rename(std::string_view to)"));
    // Virtual methods are implemented in C++: overriding them must keep owned types.
    assert!(h.contains("virtual std::string push(const std::string& s) = 0;"));
    assert!(h.contains("virtual std::string label(const std::string& prefix) = 0;"));
}

// clang-tidy (performance-move-const-arg): trivially copyable values are copied, the rest moved.
#[test]
fn only_values_that_are_not_trivially_copyable_are_moved() {
    let h = generate().header;
    assert!(h.contains("code(code_)"), "int32_t field is copied");
    assert!(
        h.contains("return ::sample::Point{f0, f1, std::move(f2)};"),
        "doubles copied, optional<string> moved"
    );
    assert!(
        h.contains("::sample::Shape::Circle{std::move(f0), f1}"),
        "a record holding a string is moved"
    );
}

#[test]
fn display_types_get_a_std_formatter_where_format_exists() {
    let udl = r#"
namespace shown { };
[Traits=(Display)] interface Thing { constructor(); };
[Traits=(Display)] dictionary Point { i32 x; };
"#;
    let h = generate_from_udl(udl, "shown", &Config::default())
        .expect("generates")
        .header;
    assert!(h.contains("#if defined(__cpp_lib_format)"));
    assert!(h.contains("struct formatter<::shown::Thing> : formatter<string_view>"));
    // A header without Display types has no <format> block at all.
    let plain = generate_from_udl("namespace plain { u8 f(); };", "plain", &Config::default())
        .expect("generates")
        .header;
    assert!(!plain.contains("__cpp_lib_format"));
}
