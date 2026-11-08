#!/usr/bin/env nu
# ── uniffi-bindgen-cpp · clang-tidy ──────────────────────────────────────────
# Generates C++ for the tour fixture (which uses every feature) and runs
# clang-tidy (.clang-tidy: checks, and why some are off) on the generated source,
# header and runtime. Any finding fails. `CLANG_TIDY` overrides the executable.

use cxx.nu

const FIXTURE = "fixtures/tour"

# clang-tidy's compiler arguments: C++17, the bindings' directory, and on macOS the SDK.
export def tidy_args [out_dir: string, os: string, sdk: string = ""] {
    let base = ["--" "-std=c++17" $"-I($out_dir)"]
    if $os == "macos" and $sdk != "" { $base | append ["-isysroot" $sdk] } else { $base }
}

# Both kinds of bindings: the default, and --no-exceptions built with -fno-exceptions.
export const MODES = [
    {name: "exceptions", generate: [], cxx: []}
    {name: "no exceptions", generate: ["--no-exceptions"], cxx: ["-fno-exceptions"]}
]

def main [] {
    let root = ($env.FILE_PWD | path dirname | path dirname)
    cd $root
    ^cargo build --manifest-path $"($FIXTURE)/Cargo.toml"
    let lib = ($root | path join $FIXTURE target debug (cxx library_file "tour" $nu.os-info.name))
    let sdk = if $nu.os-info.name == "macos" { ^xcrun --show-sdk-path | str trim } else { "" }
    let tidy = ($env.CLANG_TIDY? | default "clang-tidy")
    for mode in $MODES {
        let out_dir = (mktemp --directory)
        ^cargo run --quiet -p uniffi-bindgen-cpp -- generate $lib --out-dir $out_dir --no-format --metadata-no-deps ...$mode.generate
        print -e $"▶ ($tidy) on the generated tour bindings \(($mode.name)\)"
        ^$tidy $"--config-file=($root | path join .clang-tidy)" ($out_dir | path join tour.cpp) ...(tidy_args $out_dir $nu.os-info.name $sdk) ...$mode.cxx
        rm -rf $out_dir
    }
    print -e "✔ clang-tidy: no findings"
}
