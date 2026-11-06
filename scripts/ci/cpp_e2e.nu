#!/usr/bin/env nu
# ── uniffi-bindgen-cpp · C++ end-to-end test ─────────────────────────────────
#
# Builds the `tour` fixture crate, generates C++ bindings for it, compiles them
# with a C++17 compiler (warnings are errors) and runs fixtures/tour/cpp/test_tour.cpp.
#
# Usage: nu scripts/ci/cpp_e2e.nu [--sanitize]

const FIXTURE = "fixtures/tour"

# File name of the fixture's shared library on `os`.
export def library_name [os: string] {
    match $os {
        "macos" => "libtour.dylib"
        "windows" => "tour.dll"
        _ => "libtour.so"
    }
}

# The C++ compiler to use: $CXX, else the first of c++, clang++, g++ that exists.
export def compiler [cxx: string, available: list<string>] {
    if $cxx != "" { return $cxx }
    let found = ["c++" "clang++" "g++"] | where { |c| $c in $available }
    if ($found | is-empty) { error make {msg: "no C++ compiler found (set $CXX)"} }
    $found | first
}

# Arguments that compile the generated bindings and the test into `output`.
export def compile_args [out_dir: string, lib_dir: string, output: string, sanitize: bool] {
    let sanitizers = if $sanitize { ["-fsanitize=address,undefined" "-fno-omit-frame-pointer"] } else { [] }
    [
        "-std=c++17" "-Wall" "-Wextra" "-Werror" "-g"
        ...$sanitizers
        $"($out_dir)/tour.cpp" $"($out_dir)/test_tour.cpp"
        $"-I($out_dir)"
        $"-L($lib_dir)" "-ltour" $"-Wl,-rpath,($lib_dir)"
        "-o" $output
    ]
}

def available_compilers [] {
    ["c++" "clang++" "g++"] | where { |c| (which $c | is-not-empty) }
}

def main [--sanitize] {
    let root = ($env.FILE_PWD | path dirname | path dirname)
    cd $root
    print -e "▶ building the tour fixture"
    ^cargo build --manifest-path $"($FIXTURE)/Cargo.toml"
    let lib_dir = ($root | path join $FIXTURE target debug)
    let lib = ($lib_dir | path join (library_name $nu.os-info.name))
    let out_dir = (mktemp --directory)

    print -e "▶ generating bindings"
    ^cargo run --quiet -p uniffi-bindgen-cpp -- generate $lib --out-dir $out_dir --no-format --metadata-no-deps
    cp $"($FIXTURE)/cpp/test_tour.cpp" $out_dir

    let cxx = (compiler ($env.CXX? | default "") (available_compilers))
    let output = ($out_dir | path join test_tour)
    print -e $"▶ compiling with ($cxx)"
    ^$cxx ...(compile_args $out_dir $lib_dir $output $sanitize)

    print -e "▶ running"
    with-env {ASAN_OPTIONS: "detect_leaks=0"} { ^$output }
    print -e "✔ C++ end-to-end test passed"
}
