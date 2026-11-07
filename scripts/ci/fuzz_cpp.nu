#!/usr/bin/env nu
# ── uniffi-bindgen-cpp · fuzz the C++ runtime ────────────────────────────────
#
# Fuzzes the decoding of values from Rust (fuzz/cpp/fuzz_codecs.cpp) under
# ASan and UBSan: with libFuzzer for --secs seconds when clang has it (Linux),
# else with the built-in pseudo-random driver.
#
# Usage: nu scripts/ci/fuzz_cpp.nu [--secs 60]

use cxx.nu

const SOURCE = "fuzz/cpp/fuzz_codecs.cpp"
const RUNTIME = "crates/uniffi-bindgen-cpp/src/runtime"

# Compiler arguments: libFuzzer, or the smoke driver.
export def build_args [libfuzzer: bool, output: string] {
    let fuzzer = if $libfuzzer { "-fsanitize=fuzzer,address,undefined" } else { "-fsanitize=address,undefined" }
    let smoke = if $libfuzzer { [] } else { ["-DUNIFFI_FUZZ_SMOKE"] }
    ["-std=c++17" "-O1" "-g" $fuzzer "-fno-sanitize-recover=all" ...$smoke $"-I($RUNTIME)" $SOURCE "-o" $output]
}

# Arguments for running the fuzzer.
export def run_args [libfuzzer: bool, secs: int, corpus: string] {
    if $libfuzzer {
        [$corpus $"-max_total_time=($secs)" "-max_len=4096"]
    } else {
        ["1000000"]
    }
}

def main [--secs: int = 60] {
    let root = ($env.FILE_PWD | path dirname | path dirname)
    cd $root
    let compiler = (cxx compiler ($env.CXX? | default "") (["clang++"] | append (cxx available) | uniq | where { |c| (which $c | is-not-empty) }))
    let out = (mktemp --directory)
    let exe = ($out | path join fuzz_codecs)
    # libFuzzer ships with LLVM's clang, not with Apple's.
    let libfuzzer = (do { ^$compiler ...(build_args true $exe) } | complete).exit_code == 0
    if not $libfuzzer {
        print -e "▶ no libFuzzer: using the pseudo-random driver"
        ^$compiler ...(build_args false $exe)
    }
    let corpus = ($out | path join corpus)
    mkdir $corpus
    ^$exe ...(run_args $libfuzzer $secs $corpus)
    print -e "✔ C++ runtime fuzzing found nothing"
}
