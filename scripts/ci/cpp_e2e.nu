#!/usr/bin/env nu
# ── uniffi-bindgen-cpp · C++ end-to-end test ─────────────────────────────────
#
# Builds the `tour` fixture crate, generates C++ bindings for it, compiles them
# with a C++17 compiler (warnings are errors) and runs fixtures/tour/cpp/test_tour.cpp.
#
# Usage: nu scripts/ci/cpp_e2e.nu [--sanitize]

const FIXTURE = "fixtures/tour"

use cxx.nu

# The C++ test: bindings generated with --no-exceptions have their own.
export def test_file [exceptions: bool] {
    if $exceptions { "test_tour.cpp" } else { "test_tour_noexcept.cpp" }
}

# Arguments that compile the generated bindings and the test into `output`;
# without exceptions, with -fno-exceptions.
export def compile_args [out_dir: string, lib_dir: string, output: string, sanitize: bool, exceptions: bool = true] {
    let no_exceptions = if $exceptions { [] } else { ["-fno-exceptions"] }
    [
        ...(cxx flags $sanitize)
        ...$no_exceptions
        $"($out_dir)/tour.cpp" $"($out_dir)/(test_file $exceptions)"
        $"-I($out_dir)"
        ...(cxx link_args $lib_dir "tour")
        "-o" $output
    ]
}

def main [--sanitize, --no-exceptions] {
    let root = ($env.FILE_PWD | path dirname | path dirname)
    cd $root
    print -e "▶ building the tour fixture"
    ^cargo build --manifest-path $"($FIXTURE)/Cargo.toml"
    let lib_dir = ($root | path join $FIXTURE target debug)
    let lib = ($lib_dir | path join (cxx library_file "tour" $nu.os-info.name))
    let out_dir = (mktemp --directory)

    print -e "▶ generating bindings"
    let mode = if $no_exceptions { ["--no-exceptions"] } else { [] }
    ^cargo run --quiet -p uniffi-bindgen-cpp -- generate $lib --out-dir $out_dir --no-format --metadata-no-deps ...$mode
    cp $"($FIXTURE)/cpp/(test_file (not $no_exceptions))" $out_dir

    let compiler = (cxx compiler ($env.CXX? | default "") (cxx available))
    let output = ($out_dir | path join test_tour)
    print -e $"▶ compiling with ($compiler)"
    ^$compiler ...(compile_args $out_dir $lib_dir $output $sanitize (not $no_exceptions))

    print -e "▶ running"
    with-env {ASAN_OPTIONS: "detect_leaks=0"} { ^$output }
    print -e "✔ C++ end-to-end test passed"
}
