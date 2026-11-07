# ── uniffi-bindgen-cpp · C++ toolchain helpers ───────────────────────────────
# Shared by cpp_e2e.nu and fixtures.nu: compiler choice, flags, library names.

# File name of the shared library `name` on `os`.
export def library_file [name: string, os: string] {
    match $os {
        "macos" => $"lib($name).dylib"
        "windows" => $"($name).dll"
        _ => $"lib($name).so"
    }
}

# The C++ compiler to use: $CXX, else the first of c++, clang++, g++ that exists.
export def compiler [cxx: string, available: list<string>] {
    if $cxx != "" { return $cxx }
    let found = ["c++" "clang++" "g++"] | where { |c| $c in $available }
    if ($found | is-empty) { error make {msg: "no C++ compiler found (set $CXX)"} }
    $found | first
}

# Flags every compile uses: C++17, all warnings, warnings are errors.
export def flags [sanitize: bool] {
    let sanitizers = if $sanitize { ["-fsanitize=address,undefined" "-fno-omit-frame-pointer"] } else { [] }
    ["-std=c++17" "-Wall" "-Wextra" "-Werror" "-g" ...$sanitizers]
}

# Linker arguments for the Rust library `name` in `dir`.
export def link_args [dir: string, name: string] {
    [$"-L($dir)" $"-l($name)" $"-Wl,-rpath,($dir)"]
}

# The compilers that are installed.
export def available [] {
    ["c++" "clang++" "g++"] | where { |c| (which $c | is-not-empty) }
}
