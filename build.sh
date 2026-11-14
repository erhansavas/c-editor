#!/bin/zsh
# ./build.sh [release|debug|test|measure|dist] [--install]
#   release  the app, optimised for this Mac's own processor (default)
#   debug    the app under ASan/UBSan
#   test     the tests under ASan/UBSan
#   measure  what a keystroke and a program's build cost, in a build like release (tools/measure.mm)
#   dist     the Universal app (Apple silicon and Intel) in a DMG with its SHA-256, in build/dist
#   --install  also puts the app that was built in /Applications
set -euo pipefail
cd -- "${0:A:h}"
config=release
install=0
for arg in "$@"; do
    case $arg in
        release|debug|test|measure|dist) config=$arg ;;
        --install) install=1 ;;
        *) print -u2 -- "usage: ./build.sh [release|debug|test|measure|dist] [--install]"; exit 2 ;;
    esac
done
readonly sdk="$(/usr/bin/xcrun --sdk macosx --show-sdk-path)"
readonly cxx="$(/usr/bin/xcrun -f clang++)"
readonly cc="$(/usr/bin/xcrun -f clang)"
# The oldest macOS the app runs on. Info.plist is the one place that says it.
readonly min_macos="$(/usr/libexec/PlistBuddy -c 'Print :LSMinimumSystemVersion' resources/Info.plist)"
readonly warnings=(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror)
readonly common=(-isysroot "$sdk" -mmacosx-version-min=$min_macos -std=c++23 -fno-exceptions -fno-rtti "${warnings[@]}" -Isrc)
readonly checked=(-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
readonly optimised=(-O3 -flto=full -DNDEBUG)
readonly portable=(src/core/*.cpp src/posix/*.cpp)
readonly frameworks=(-framework AppKit -framework CoreText -framework UniformTypeIdentifiers)
readonly out=build/$config
# Every build but dist is for the processor it runs on; under `arch -x86_64` that is Intel on an Apple silicon Mac too.
[[ $config == dist ]] && readonly archs=(arm64 x86_64) || readonly archs=("$(uname -m)")

# compile_link <output> <arch> <sources...> -- <link arguments...>: every source on its own core, then the link.
compile_link() {
    local output=$1 arch=$2 failed=0
    local -a sources objects pids
    shift 2
    while [[ $1 != -- ]]; do sources+=("$1"); shift; done
    shift
    mkdir -p "$out/obj/$arch"
    for source in $sources; do
        objects+=("$out/obj/$arch/${${source//\//_}%.*}.o")
        "$cxx" "${common[@]}" -arch $arch "${flags[@]}" -c "$source" -o "$objects[-1]" &
        pids+=($!)
    done
    for pid in $pids; do wait $pid || failed=1; done
    (( ! failed )) || exit 1
    "$cxx" "${common[@]}" -arch $arch "${flags[@]}" "${objects[@]}" "$@" -o "$output"
}

# The folder programs are compiled against: the headers they see and the source of the runtime they link, which the
# Mac's own compiler builds (src/posix/compile.cpp). It is checked here, under this build's warnings.
install_compat() {
    local arch
    for arch in $archs; do
        "$cc" -isysroot "$sdk" -arch $arch -mmacosx-version-min=$min_macos -std=gnu11 -O2 -fno-builtin -D_FORTIFY_SOURCE=0 \
            "${warnings[@]}" -fsyntax-only src/runtime/windows_runtime.c
    done
    rm -rf "$1"
    /usr/bin/ditto resources/compat "$1"
    cp -p src/runtime/windows_runtime.c "$1/"
}

if [[ $config == test ]]; then
    flags=("${checked[@]}")
    compile_link "$out/tests" $archs "${portable[@]}" tests/*.cpp -- -liconv
    mkdir -p build/tmp
    install_compat "$out/compat"
    # tests/local is yours and stays out of git: every program in it, or in a folder linked there, must build.
    exec "$out/tests" "$PWD/$out/compat" "$PWD/build/tmp" "$PWD/tests/local"
fi

# What each kind of build adds for the processor it is for.
flags_for() {
    case $1-$2 in
        debug-*) flags=("${checked[@]}" -fobjc-arc) ;;
        dist-arm64) flags=("${optimised[@]}" -mcpu=apple-m1 -fobjc-arc) ;;  # the oldest Apple silicon
        dist-x86_64) flags=("${optimised[@]}" -fobjc-arc) ;;                # the compiler's baseline: every Intel Mac
        *-arm64) flags=("${optimised[@]}" -mcpu=native -fobjc-arc) ;;
        *-x86_64) flags=("${optimised[@]}" -march=native -fobjc-arc) ;;
    esac
}

if [[ $config == measure ]]; then
    flags_for release $archs
    views=(src/platform/macos/*.mm)
    compile_link "$out/measure" $archs "${portable[@]}" ${views:#*/main.mm} tools/measure.mm -- "${frameworks[@]}"
    mkdir -p build/tmp
    install_compat "$out/compat"
    exec "$out/measure" "$PWD/$out/compat" "$PWD/build/tmp"
fi

readonly app="$out/C Editor.app"
readonly binary="$app/Contents/MacOS/c-editor"
rm -rf "$app"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"
for arch in $archs; do
    flags_for $config $arch
    compile_link "$out/obj/$arch/c-editor" $arch "${portable[@]}" src/platform/macos/*.mm -- "${frameworks[@]}"
done
/usr/bin/lipo -create "$out"/obj/${^archs}/c-editor -output "$binary"
cp resources/Info.plist "$app/Contents/Info.plist"
cp resources/CSource.icns "$app/Contents/Resources/CSource.icns"
install_compat "$app/Contents/Resources/compat"
# Absolute paths: actool hands the work to a helper that may still sit in another project's folder.
/usr/bin/xcrun actool "$PWD/resources/AppIcon.icon" --compile "$PWD/$app/Contents/Resources" --platform macosx \
    --minimum-deployment-target $min_macos --app-icon AppIcon --output-partial-info-plist "$PWD/$out/icon-info.plist" >/dev/null
/usr/bin/codesign --force --sign - --timestamp=none "$app"
print -- "built $app ($(/usr/bin/lipo -archs "$binary"), macOS $min_macos+)"

if [[ $config == dist ]]; then
    readonly version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' resources/Info.plist)"
    readonly dmg="C-Editor-$version.dmg"
    readonly stage="$out/stage"
    rm -rf "$stage"
    mkdir -p "$stage"
    /usr/bin/ditto "$app" "$stage/C Editor.app"
    ln -s /Applications "$stage/Applications"
    rm -f "$out/$dmg"
    if ! log=$(/usr/sbin/diskutil image create from --format ULFO --volumeName "C Editor" "$stage" "$out/$dmg" 2>&1); then
        print -u2 -- "$log"
        exit 1
    fi
    (cd "$out" && /usr/bin/shasum -a 256 "$dmg" > "$dmg.sha256")
    print -- "packed $out/$dmg"
fi
if (( install )); then
    readonly installed="/Applications/C Editor.app"
    # The app that is there goes to the Trash whole: copying over it would keep files this build no longer has.
    [[ -e $installed ]] && mv "$installed" "$HOME/.Trash/C Editor $(date +%Y%m%d-%H%M%S).app"
    /usr/bin/ditto "$app" "$installed"
    /System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f "$installed"
    print -- "installed $installed"
fi
