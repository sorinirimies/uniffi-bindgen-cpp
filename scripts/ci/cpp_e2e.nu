#!/usr/bin/env nu
# ── uniffi-bindgen-cpp · C++ end-to-end test ─────────────────────────────────
#
# Builds the `tour` fixture crate, generates C++ bindings for it, compiles them
# with a C++17 compiler (warnings are errors) and runs fixtures/tour/cpp/test_tour.cpp.
#
# Usage: nu scripts/ci/cpp_e2e.nu [--sanitize]

const FIXTURE = "fixtures/tour"

use cxx.nu

# Arguments that compile the generated bindings and the test into `output`.
export def compile_args [out_dir: string, lib_dir: string, output: string, sanitize: bool] {
    [
        ...(cxx flags $sanitize)
        $"($out_dir)/tour.cpp" $"($out_dir)/test_tour.cpp"
        $"-I($out_dir)"
        ...(cxx link_args $lib_dir "tour")
        "-o" $output
    ]
}

def main [--sanitize] {
    let root = ($env.FILE_PWD | path dirname | path dirname)
    cd $root
    print -e "▶ building the tour fixture"
    ^cargo build --manifest-path $"($FIXTURE)/Cargo.toml"
    let lib_dir = ($root | path join $FIXTURE target debug)
    let lib = ($lib_dir | path join (cxx library_file "tour" $nu.os-info.name))
    let out_dir = (mktemp --directory)

    print -e "▶ generating bindings"
    ^cargo run --quiet -p uniffi-bindgen-cpp -- generate $lib --out-dir $out_dir --no-format --metadata-no-deps
    cp $"($FIXTURE)/cpp/test_tour.cpp" $out_dir

    let compiler = (cxx compiler ($env.CXX? | default "") (cxx available))
    let output = ($out_dir | path join test_tour)
    print -e $"▶ compiling with ($compiler)"
    ^$compiler ...(compile_args $out_dir $lib_dir $output $sanitize)

    print -e "▶ running"
    with-env {ASAN_OPTIONS: "detect_leaks=0"} { ^$output }
    print -e "✔ C++ end-to-end test passed"
}
