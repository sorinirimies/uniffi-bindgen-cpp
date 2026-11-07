#!/usr/bin/env nu
# ── uniffi-bindgen-cpp · benchmarks ──────────────────────────────────────────
#
# Runs uniffi's benchmark fixture through the C++ bindings, optimized: Rust's
# Criterion times calls from C++ into Rust and back. The numbers compare with
# uniffi's own `cargo bench` for Kotlin, Swift and Python.
#
# Usage: nu scripts/bench.nu [criterion args, e.g. a name filter or --save-baseline x]

use ci/cxx.nu

const FIXTURES = "fixtures/uniffi"
const LIB = "uniffi_benchmarks"

# Compiler arguments for the optimized benchmark program.
export def bench_args [out: string, lib_dir: string, output: string] {
    [
        "-std=c++17" "-O2" "-DNDEBUG"
        $"($out)/benchmarks.cpp" $"($FIXTURES)/benchmarks/cpp/bench_benchmarks.cpp"
        $"-I($out)" ...(cxx link_args $lib_dir $LIB) "-o" $output
    ]
}

def --wrapped main [...criterion: string] {
    let root = ($env.FILE_PWD | path dirname)
    cd $root
    ^cargo build --quiet --release --manifest-path ($FIXTURES | path join Cargo.toml) -p uniffi-fixture-benchmarks
    ^cargo build --quiet --release -p uniffi-bindgen-cpp
    let lib_dir = ($root | path join $FIXTURES target release)
    let out = (mktemp --directory)
    do {
        cd ($FIXTURES | path join benchmarks)
        ^($root | path join target release uniffi-bindgen-cpp) generate ($lib_dir | path join (cxx library_file $LIB $nu.os-info.name)) --out-dir $out --no-format
    }
    let exe = ($out | path join bench)
    let compiler = (cxx compiler ($env.CXX? | default "") (cxx available))
    ^$compiler ...(bench_args $out $lib_dir $exe)
    ^$exe ...$criterion
}
