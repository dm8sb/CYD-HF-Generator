#!/usr/bin/env bash
# Builds the fuzz harnesses in test/fuzz (W-S25). See test/fuzz/README.md.
#
#   test/fuzz/build.sh [--standalone] [target ...]
#
# Default: libFuzzer with ASan/UBSan (clang, Linux). --standalone: plain main from
# standalone_main.cpp with -DFUZZ_STANDALONE in C++11 (gcc, MinGW, clang; SANITIZE=1 adds
# -fsanitize=address,undefined). Without targets all are built. Binaries: $OUT/fuzz_<target>.
#
# Environment: CC, CXX (default clang/clang++, standalone gcc/g++), OUT (default .pio/fuzz),
# SCPI_DIR (scpi-parser checkout; default $OUT/scpi-parser, fetched with git at the pinned
# commit if missing), EXTRA_FLAGS (appended to every compile).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

# The commit platformio.ini pins in [native_test] lib_deps (S-20).
SCPI_COMMIT=886615902b2777b71860445305bfe5a3faef4f3e
SCPI_URL=https://github.com/j123b567/scpi-parser.git

ALL_TARGETS="line_framer sanitize scpi scpi_number remote_frequency net_settings scan_list"

MODE=libfuzzer
TARGETS=""
for arg in "$@"; do
    case "$arg" in
        --standalone) MODE=standalone ;;
        -h|--help) sed -n '2,11p' "$0"; exit 0 ;;
        -*) echo "unknown option $arg" >&2; exit 2 ;;
        *) TARGETS="$TARGETS $arg" ;;
    esac
done
TARGETS="${TARGETS:-$ALL_TARGETS}"

OUT="${OUT:-$ROOT/.pio/fuzz}"
SCPI_DIR="${SCPI_DIR:-$OUT/scpi-parser}"
EXTRA_FLAGS="${EXTRA_FLAGS:-}"
mkdir -p "$OUT/obj-$MODE"

# Sources per target (besides the harness).
sources() {
    case "$1" in
        line_framer) echo "src/remote/line_framer.cpp" ;;
        sanitize) echo "src/utils/sanitize.cpp" ;;
        scpi) echo "src/remote/scpi.cpp src/remote/line_framer.cpp src/utils/logger.cpp" ;;
        scpi_number) echo "src/remote/scpi.cpp src/utils/logger.cpp" ;;
        remote_frequency) echo "src/utils/frequency_utils.cpp" ;;
        net_settings) echo "src/net/net_settings.cpp src/services/storage.cpp src/utils/logger.cpp" ;;
        scan_list) echo "src/net/scan_list.cpp src/utils/sanitize.cpp" ;;
        *) echo "unknown target $1" >&2; exit 2 ;;
    esac
}
needs_scpi() { [ "$1" = scpi ] || [ "$1" = scpi_number ]; }

fetch_scpi() {
    if [ -f "$SCPI_DIR/libscpi/inc/scpi/scpi.h" ]; then
        if [ -d "$SCPI_DIR/.git" ]; then
            have="$(git -C "$SCPI_DIR" rev-parse HEAD)"
            if [ "$have" != "$SCPI_COMMIT" ]; then
                echo "scpi-parser in $SCPI_DIR is at $have, expected $SCPI_COMMIT" >&2
                exit 1
            fi
        fi
        return
    fi
    echo "fetching scpi-parser $SCPI_COMMIT into $SCPI_DIR"
    rm -rf "$SCPI_DIR"
    mkdir -p "$SCPI_DIR"
    git -C "$SCPI_DIR" init -q
    git -C "$SCPI_DIR" remote add origin "$SCPI_URL"
    git -C "$SCPI_DIR" fetch -q --depth 1 origin "$SCPI_COMMIT"
    git -C "$SCPI_DIR" checkout -q FETCH_HEAD
    [ "$(git -C "$SCPI_DIR" rev-parse HEAD)" = "$SCPI_COMMIT" ] || { echo "scpi-parser commit mismatch" >&2; exit 1; }
}

COMMON="-g -Wall -DSCPI_USER_CONFIG=1 -I include -I src -I test/fakes -I $SCPI_DIR/libscpi/inc"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) WINDOWS=1 ;;
    *) WINDOWS=0 ;;
esac

if [ "$MODE" = libfuzzer ]; then
    CC="${CC:-clang}"
    CXX="${CXX:-clang++}"
    SAN="-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer"
    # The C library calls the C++ command handlers through function pointers; clang's
    # -fsanitize=function would compare C and C++ type hashes there, so it is off for C only.
    CFLAGS="$COMMON -O1 $SAN -fno-sanitize=function -fsanitize=fuzzer-no-link -std=gnu11"
    CXXFLAGS="$COMMON -O1 $SAN -fsanitize=fuzzer -std=gnu++17"
    MAIN=""
else
    CC="${CC:-gcc}"
    CXX="${CXX:-g++}"
    SAN=""
    if [ "${SANITIZE:-0}" = 1 ]; then SAN="-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer"; fi
    OPT="-O1 $SAN"
    if [ "$WINDOWS" = 1 ]; then
        # As env:native-win: printf with %llu, scpi-parser's hidden attributes, no runtime DLLs.
        OPT="$OPT -D__USE_MINGW_ANSI_STDIO=1 -Wno-attributes -static"
    fi
    CFLAGS="$COMMON $OPT -std=gnu11"
    CXXFLAGS="$COMMON $OPT -std=gnu++11 -DFUZZ_STANDALONE"
    MAIN="test/fuzz/standalone_main.cpp"
fi

SCPI_OBJS=""
build_scpi_lib() {
    fetch_scpi
    SCPI_OBJS=""
    for c in "$SCPI_DIR"/libscpi/src/*.c; do
        o="$OUT/obj-$MODE/scpi-$(basename "${c%.c}").o"
        # shellcheck disable=SC2086
        "$CC" $CFLAGS $EXTRA_FLAGS -c "$c" -o "$o"
        SCPI_OBJS="$SCPI_OBJS $o"
    done
}

for t in $TARGETS; do
    srcs="$(sources "$t")"
    libs=""
    if needs_scpi "$t"; then
        [ -n "$SCPI_OBJS" ] || build_scpi_lib
        libs="$SCPI_OBJS"
    fi
    exe="$OUT/fuzz_$t"
    [ "$WINDOWS" = 1 ] && exe="$exe.exe"
    echo "building $exe ($MODE)"
    # shellcheck disable=SC2086
    "$CXX" $CXXFLAGS $EXTRA_FLAGS "test/fuzz/fuzz_$t.cpp" $srcs $MAIN $libs -o "$exe"
done
