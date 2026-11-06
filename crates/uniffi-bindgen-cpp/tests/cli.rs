//! End-to-end: the real binary, real arguments, real files.

use std::process::Command;

fn bin() -> Command {
    Command::new(env!("CARGO_BIN_EXE_uniffi-bindgen-cpp"))
}

fn temp_dir(name: &str) -> std::path::PathBuf {
    let dir =
        std::env::temp_dir().join(format!("uniffi-bindgen-cpp-{name}-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(&dir).expect("create temp dir");
    dir
}

#[test]
fn version_matches_the_crate() {
    let out = bin().arg("--version").output().expect("run");
    assert!(out.status.success());
    let text = String::from_utf8_lossy(&out.stdout);
    assert_eq!(
        text.trim(),
        format!("uniffi-bindgen-cpp {}", env!("CARGO_PKG_VERSION"))
    );
}

#[test]
fn unknown_argument_is_a_usage_error() {
    let out = bin().arg("--nope").output().expect("run");
    assert_eq!(out.status.code(), Some(2));
}

#[test]
fn runtime_subcommand_writes_the_header() {
    let dir = temp_dir("runtime");
    let out = bin().arg("runtime").arg(&dir).output().expect("run");
    assert!(
        out.status.success(),
        "{}",
        String::from_utf8_lossy(&out.stderr)
    );
    let header = std::fs::read_to_string(dir.join("uniffi_runtime.hpp")).expect("header written");
    assert_eq!(header, uniffi_bindgen_cpp::RUNTIME_HEADER);
    let _ = std::fs::remove_dir_all(dir);
}

#[test]
fn generate_rejects_sources_it_cannot_read() {
    let dir = temp_dir("reject");
    let out = bin()
        .args(["generate", "Cargo.toml", "--out-dir"])
        .arg(&dir)
        .output()
        .expect("run");
    assert!(!out.status.success());
    assert!(String::from_utf8_lossy(&out.stderr).contains("neither a library nor a .udl file"));
    let _ = std::fs::remove_dir_all(dir);
}
