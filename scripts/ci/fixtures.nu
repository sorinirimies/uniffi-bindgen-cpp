#!/usr/bin/env nu
# ── uniffi-bindgen-cpp · uniffi fixture suite ────────────────────────────────
#
# Runs uniffi's own test fixtures (fixtures/uniffi, vendored from uniffi-rs)
# against the C++ bindings:
#   1. builds every fixture crate,
#   2. generates C++ for every cdylib and compiles it (warnings are errors),
#   3. compiles, links and runs each fixture's cpp/test_*.cpp.
#
# Usage: nu scripts/ci/fixtures.nu [--sanitize] [--only <cdylib name>]

use cxx.nu

const FIXTURES = "fixtures/uniffi"

# Libraries we only build: upstream does not generate bindings for them either.
export const SKIP = [
    # Its UDL lives in another crate, so library mode cannot find the UDL types.
    "uniffi_proc_macro_nip"
]

# The cdylib targets in `cargo metadata` output: name and package directory.
export def cdylibs [metadata: record] {
    $metadata.packages | each { |p|
        $p.targets | where { |t| "cdylib" in $t.crate_types } | each { |t|
            {name: $t.name, dir: ($p.manifest_path | path dirname)}
        }
    } | flatten | sort-by name
}

# The libraries to run: all but SKIP, or just `only`.
export def selected [libs: list, only: string] {
    $libs | where { |l| $l.name not-in $SKIP and ($only == "" or $l.name == $only) }
}

# Generator arguments and extra compiler flags for a mode: without exceptions the
# bindings are generated with --no-exceptions and must build with -fno-exceptions.
export def mode_args [exceptions: bool] {
    if $exceptions { {generate: [], cxx: []} } else { {generate: ["--no-exceptions"], cxx: ["-fno-exceptions"]} }
}

def run_library [lib: record, bin: string, lib_dir: string, out_root: string, compiler: string, sanitize: bool, exceptions: bool] {
    let mode = (mode_args $exceptions)
    let out = ($out_root | path join $lib.name)
    mkdir $out
    let library = ($lib_dir | path join (cxx library_file $lib.name $nu.os-info.name))
    # Library mode finds UDL files through `cargo metadata`, so run inside the fixture workspace.
    let gen = (do { cd $lib.dir; ^$bin generate $library --out-dir $out --no-format ...$mode.generate } | complete)
    if $gen.exit_code != 0 { return [{library: $lib.name, step: "generate", detail: $gen.stderr}] }

    let flags = (cxx flags $sanitize) ++ $mode.cxx
    let sources = (glob $"($out)/*.cpp")
    mut objects = []
    mut failures = []
    for source in $sources {
        let object = ($source | str replace -r '\.cpp$' '.o')
        let r = (do { ^$compiler ...$flags -c $source $"-I($out)" -o $object } | complete)
        if $r.exit_code != 0 {
            $failures = ($failures | append {library: $lib.name, step: $"compile ($source | path basename)", detail: $r.stderr})
        }
        $objects = ($objects | append $object)
    }
    if ($failures | is-not-empty) { return $failures }
    # The fixtures' tests catch exceptions: without them, building the bindings is the test.
    if not $exceptions { return [] }
    let objects = $objects

    let check_dir = ($env.FILE_PWD | path dirname | path dirname | path join $FIXTURES cpp)
    for test in (glob $"($lib.dir)/cpp/test_*.cpp") {
        let exe = ($out | path join ($test | path parse | get stem))
        let test_flags = (cxx test_flags $test $sanitize)
        let r = (do { ^$compiler ...$test_flags $test ...$objects $"-I($out)" $"-DUNIFFI_GENERATED_DIR=\"($out)\"" $"-I($check_dir)" ...(cxx link_args $lib_dir $lib.name) -o $exe } | complete)
        if $r.exit_code != 0 {
            $failures = ($failures | append {library: $lib.name, step: $"build ($test | path basename)", detail: $r.stderr})
            continue
        }
        let run = (with-env {ASAN_OPTIONS: "detect_leaks=0"} { do { ^$exe } | complete })
        print -e $"  ($lib.name)/($test | path basename): ($run.stdout | str trim)"
        if $run.exit_code != 0 {
            $failures = ($failures | append {library: $lib.name, step: $"run ($test | path basename)", detail: $"($run.stdout)($run.stderr)"})
        }
    }
    # Benchmarks are only built here; `just bench` runs them.
    for bench in (glob $"($lib.dir)/cpp/bench_*.cpp") {
        let exe = ($out | path join ($bench | path parse | get stem))
        let r = (do { ^$compiler ...$flags $bench ...$objects $"-I($out)" ...(cxx link_args $lib_dir $lib.name) -o $exe } | complete)
        if $r.exit_code != 0 {
            $failures = ($failures | append {library: $lib.name, step: $"build ($bench | path basename)", detail: $r.stderr})
        }
    }
    $failures
}

def main [--sanitize, --no-exceptions, --only: string = ""] {
    let root = ($env.FILE_PWD | path dirname | path dirname)
    cd $root
    let manifest = ($FIXTURES | path join Cargo.toml)
    print -e "▶ building the fixtures"
    ^cargo build --quiet --manifest-path $manifest
    ^cargo build --quiet -p uniffi-bindgen-cpp
    let bin = ($root | path join target debug uniffi-bindgen-cpp)
    let lib_dir = ($root | path join $FIXTURES target debug)
    let libs = (selected (cdylibs (^cargo metadata --format-version 1 --no-deps --manifest-path $manifest | from json)) $only)
    let compiler = (cxx compiler ($env.CXX? | default "") (cxx available))
    let out_root = (mktemp --directory)

    let what = if $no_exceptions { "compiling with -fno-exceptions" } else { "compiling and testing" }
    print -e $"▶ generating, ($what) ($libs | length) libraries with ($compiler)"
    let failures = ($libs | each { |lib| run_library $lib $bin $lib_dir $out_root $compiler $sanitize (not $no_exceptions) } | flatten)
    if ($failures | is-not-empty) {
        for f in $failures {
            print -e $"✗ ($f.library): ($f.step)\n($f.detail | lines | first 30 | str join "\n")"
        }
        error make {msg: $"($failures | length) fixture steps failed"}
    }
    print -e "✔ all fixtures passed"
}
