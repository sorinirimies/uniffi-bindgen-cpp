#!/usr/bin/env nu
# ── uniffi-bindgen-cpp · CMake examples ──────────────────────────────────────
#
# Builds the generator, then configures, builds and tests the CMake examples:
# examples/tour always, examples/stem-mqtt when given a stem-mqtt checkout.
# This is the job that runs on Windows with MSVC.
#
# Usage: nu scripts/ci/cmake_examples.nu [--stem-mqtt <checkout>]

# Path of the generator binary built by cargo for `os`.
export def bindgen_path [root: string, os: string] {
    let exe = if $os == "windows" { "uniffi-bindgen-cpp.exe" } else { "uniffi-bindgen-cpp" }
    $root | path join target debug $exe
}

# The examples to run and their extra configure arguments.
export def examples [stem_mqtt: string] {
    let tour = [{name: "tour", args: []}]
    if $stem_mqtt == "" {
        $tour
    } else {
        $tour | append {name: "stem-mqtt", args: [$"-DSTEM_MQTT_DIR=($stem_mqtt | path expand)"]}
    }
}

def main [--stem-mqtt: string = ""] {
    let root = ($env.FILE_PWD | path dirname | path dirname)
    cd $root
    ^cargo build --quiet -p uniffi-bindgen-cpp
    let bindgen = (bindgen_path $root $nu.os-info.name)
    for example in (examples $stem_mqtt) {
        let build = ($root | path join build $example.name)
        print -e $"▶ ($example.name)"
        ^cmake -S ($root | path join examples $example.name) -B $build $"-DUNIFFI_BINDGEN_CPP=($bindgen)" ...$example.args
        ^cmake --build $build --config Debug
        ^ctest --test-dir $build -C Debug --output-on-failure
    }
    print -e "✔ CMake examples passed"
}
