#!/usr/bin/env nu
# ── uniffi-bindgen-cpp · install a pinned clang-tidy ─────────────────────────
# The clang-tidy check runs on GitHub's and Gitea's runners, whose distro
# packages differ by several LLVM releases (and so do the checks' findings).
# This installs one pinned release on both: the self-contained binary from the
# clang-tidy wheel on PyPI (a zip; nothing is pip-installed), into
# ~/.local/clang-tidy-<version>, and puts it on the PATH of later CI steps.
#
# Usage: nu scripts/ci/install_clang_tidy.nu

export const VERSION = "23.1.4"

# The wheel's platform tag for an OS/architecture.
export def wheel_tag [os: string, arch: string]: nothing -> string {
    match [$os $arch] {
        ["linux" "x86_64"] => "manylinux_2_27_x86_64"
        ["linux" "aarch64"] => "manylinux_2_26_aarch64"
        ["macos" "aarch64"] => "macosx_11_0_arm64"
        ["macos" "x86_64"] => "macosx_10_9_x86_64"
        _ => { error make --unspanned { msg: $"no clang-tidy wheel for ($os)/($arch)" } }
    }
}

# The download URL of the wheel with `tag` among PyPI's files of a release.
export def wheel_url [files: list<record>, tag: string]: nothing -> string {
    let found = ($files | where { |f| ($f.filename | str ends-with ".whl") and ($f.filename | str contains $tag) })
    if ($found | is-empty) { error make --unspanned { msg: $"no clang-tidy wheel tagged ($tag)" } }
    $found | first | get url
}

def main [] {
    let dest = ($env.HOME | path join ".local" $"clang-tidy-($VERSION)")
    let bin = ($dest | path join "clang_tidy" "data" "bin")
    if not ($bin | path join "clang-tidy" | path exists) {
        let files = (http get $"https://pypi.org/pypi/clang-tidy/($VERSION)/json" | get urls)
        let url = (wheel_url $files (wheel_tag $nu.os-info.name $nu.os-info.arch))
        let wheel = (mktemp --tmpdir --suffix .zip)
        http get --raw $url | save --force $wheel
        mkdir $dest
        if (which unzip | is-not-empty) { ^unzip -q -o $wheel -d $dest } else { ^python3 -m zipfile -e $wheel $dest }
        rm $wheel
        ^chmod +x ($bin | path join "clang-tidy")
    }
    if ($env.GITHUB_PATH? | is-not-empty) { $"($bin)\n" | save --append $env.GITHUB_PATH }
    ^($bin | path join "clang-tidy") --version
}
