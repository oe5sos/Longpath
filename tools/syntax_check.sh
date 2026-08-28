#!/usr/bin/env bash
#
# Syntax-only compile of one or more sources against real Qt headers.
#
# ── Why this lives in the repository ─────────────────────────────────
#
# It used to live in /tmp. Twice in one session /tmp was cleared out
# from under it, and both times the result was worse than losing the
# tool: `loop.sh <file>` printed "No such file or directory" to stderr,
# matched zero lines of "error:", and every caller read that as a clean
# build. Two commits went out claiming "syntax verified" on the strength
# of a script that did not exist.
#
# A checker that reports success when it is absent is not a checker. So:
#
#   1. It lives here, on the same disk as the code it checks.
#   2. It SELF-TESTS before every run — compiles a file with a
#      deliberate syntax error and refuses to continue unless the
#      compiler complains about it. If the toolchain is broken or the
#      headers have gone, you find out from this, not from a green
#      result you will believe.
#   3. It exits non-zero when it cannot do its job, so no caller can
#      mistake "could not check" for "nothing wrong".
#
# ── The known baseline ───────────────────────────────────────────────
#
# qlogging.h uses a __has_feature construct the synthesised qconfig
# cannot evaluate, and it appears in every file that includes anything
# from QtCore. It is filtered out by name and by name only; anything
# else counts.
#
# Usage:  tools/syntax_check.sh src/core/Foo.cpp [more...]
#         tools/syntax_check.sh --all-changed
#
# Modification history (Longpath):
#   2026-08-10 — Created for NereusSDR (heute Longpath) by Martin Fischer, AI-assisted
#                 via Anthropic Claude (Cowork).
#   2026-08-15 — Find Qt without being told: qmake -query, then a flat
#                 include tree, then macOS frameworks (for which a flat
#                 tree of symlinks is built under .cache/qtinc).
#                 NEREUS_QTINC still overrides everything.

set -u -o pipefail

cd "$(dirname "$0")/.." || exit 2
ROOT="$PWD"

# ── Finding the Qt headers ───────────────────────────────────────────
#
# This used to be one hard-coded path from the machine the script was
# written on. On macOS it could not work at all, and the failure was the
# script's best behaviour rather than its worst: it refused and said so,
# instead of compiling against nothing and reporting a clean run.
#
# But "refuses on your machine" is only one step better than "lies on
# your machine". Homebrew installs Qt as FRAMEWORKS —
#
#     /opt/homebrew/lib/QtCore.framework/Headers/QObject
#
# — where a Linux qtbase tree has
#
#     /usr/include/qt6/QtCore/QObject
#
# and -I wants the second shape. So on macOS the script builds itself a
# flat tree of symlinks, one per framework, and points -I at that. The
# links are rebuilt on every run: it costs milliseconds, and a stale
# link into a Qt that Homebrew has since upgraded away is exactly the
# kind of quiet wrongness this script exists to prevent.
#
# The shim lives in .cache/ — already gitignored, and on the same disk
# as the code, for the reason in the header comment.
#
# Order: an explicit NEREUS_QTINC always wins; then a flat tree, which
# is what Linux and a self-built Qt give; then frameworks.

QTINC=""

# 1. The operator's own answer. Said out loud when it is set and does
#    NOT hold Qt: falling through to auto-detection without a word
#    would run the check against a Qt the operator did not choose, and
#    a typo in an exported variable is a hard thing to see afterwards.
if [ -n "${NEREUS_QTINC:-}" ]; then
    if [ -d "${NEREUS_QTINC}/QtCore" ]; then
        QTINC="$NEREUS_QTINC"
    else
        echo "syntax_check: NEREUS_QTINC=$NEREUS_QTINC has no QtCore —" >&2
        echo "  ignoring it and looking for Qt myself." >&2
    fi
fi

# Ask Qt itself where it put things, and keep the usual suspects as a
# fallback for a machine with no qmake on PATH.
qt_headers=""
qt_libs=""
if [ -z "$QTINC" ]; then
    for q in qmake6 qmake; do
        if command -v "$q" >/dev/null 2>&1; then
            qt_headers="$("$q" -query QT_INSTALL_HEADERS 2>/dev/null)"
            qt_libs="$("$q" -query QT_INSTALL_LIBS 2>/dev/null)"
            break
        fi
    done
fi

# 2. A flat include tree.
if [ -z "$QTINC" ]; then
    for cand in "$qt_headers" \
                /usr/include/qt6 /usr/include/x86_64-linux-gnu/qt6 \
                /usr/local/include/qt6; do
        if [ -n "$cand" ] && [ -d "$cand/QtCore" ]; then
            QTINC="$cand"
            break
        fi
    done
fi

# 3. macOS frameworks — build the flat tree the compiler wants.
if [ -z "$QTINC" ]; then
    for libdir in "$qt_libs" /opt/homebrew/lib /usr/local/lib \
                  /opt/homebrew/opt/qt/lib /usr/local/opt/qt/lib; do
        [ -n "$libdir" ] || continue
        [ -d "$libdir/QtCore.framework/Headers" ] || continue

        shim="$ROOT/.cache/qtinc"
        rm -rf "$shim"
        mkdir -p "$shim" || break
        for fw in "$libdir"/*.framework; do
            [ -d "$fw/Headers" ] || continue
            ln -sfn "$fw/Headers" "$shim/$(basename "$fw" .framework)"
        done
        # Qt's private rhi headers, when this Qt ships them. Homebrew
        # does not always put them directly under Headers/rhi — this
        # build (qtbase 6.11.1) nests them one level deeper, under the
        # version-numbered subdir Homebrew's Headers symlink also
        # exposes: Headers/<version>/QtGui/rhi. Try both rather than
        # picking one, since which layout a given Qt release uses isn't
        # something to hardcode. This is conditional rather than an
        # unconditional link: a dangling one would put an -I on the
        # command line that silently resolves nothing, and the first
        # symptom would be a confusing error in a file that includes
        # <rhi/qrhi.h>.
        rhi_dir="$libdir/QtGui.framework/Headers/rhi"
        if [ ! -d "$rhi_dir" ]; then
            # -L: Headers itself is a symlink (-> Versions/Current/Headers)
            # find won't descend into a symlinked starting point without it.
            rhi_dir="$(find -L "$libdir/QtGui.framework/Headers" -mindepth 3 -maxdepth 3 -type d -name rhi -print -quit 2>/dev/null)"
        fi
        if [ -n "$rhi_dir" ] && [ -d "$rhi_dir" ]; then
            ln -sfn "$rhi_dir" "$shim/rhi"
        fi

        if [ -d "$shim/QtCore" ]; then
            QTINC="$shim"
            echo "syntax_check: Qt frameworks at $libdir" >&2
            echo "  flat tree built at ${shim#$ROOT/}" >&2
        fi
        break
    done
fi

if [ -z "$QTINC" ] || [ ! -d "$QTINC/QtCore" ]; then
    echo "syntax_check: no Qt headers found." >&2
    echo "  Looked at: qmake -query, /usr/include/qt6," >&2
    echo "  and framework layouts under /opt/homebrew/lib." >&2
    echo "  Set NEREUS_QTINC to a qtbase include tree to override." >&2
    exit 2
fi

# ── PortAudio ─────────────────────────────────────────────────────────
#
# CMakeLists.txt pulls PortAudio in via FetchContent — there is no
# system package to point at, only whatever landed under a build
# directory's _deps/ the last time cmake actually configured. The
# build-dir name is not fixed (build/, build-tests/, whatever the
# operator chose), so this looks for the one landmark that is fixed —
# <build-dir>/_deps/portaudio-src/include, four levels under $ROOT —
# rather than hardcoding the build-dir name. Conditional, like the
# rhi lookup above:
# a dangling -I here would turn "AudioEngine.cpp includes portaudio.h"
# into a "file not found" that reads exactly like a real bug in the
# code and is not one. If no build has ever been run, this simply
# finds nothing and AudioEngine.cpp (and anything else that reaches
# <portaudio.h>) reports that one missing header — an honest result,
# not a silent lie.
PA_INC="$(find "$ROOT" -maxdepth 4 -type d -path '*/_deps/portaudio-src/include' -print -quit 2>/dev/null)"

# third_party/wdsp/CMakeLists.txt adds third_party/wdsp/src PUBLIC to
# wdsp_static, which LongpathObjs links -- so in the real build, any file
# reaching a bare `#include "resample.h"` (or another WDSP header included
# the same way, e.g. TciServer.cpp's RESAMPLEF wrapper) resolves it via
# that transitive include path. Unlike PA_INC this path is static -- always
# present in the tree, not something a build creates -- so no find/fallback
# is needed.
WDSP_INC="$ROOT/third_party/wdsp/src"

# ── rnnoise / libspecbleach (WDSP NR3/NR4 backends) ─────────────────────
#
# Same story as PortAudio: both are FetchContent'd, not vendored, so their
# headers only exist under a build directory's _deps/ once cmake has
# actually configured. third_party/wdsp/src/rnnr.h (NR3) and sbnr.c (NR4)
# reach these bare-name includes only when HAVE_WDSP is defined -- so this
# gap stayed invisible until HAVE_WDSP was added to the flags below
# (2026-08-26). Same conditional-find, same honest-absence rationale as
# PA_INC.
RNNOISE_INC="$(find "$ROOT" -maxdepth 4 -type d -path '*/_deps/rnnoise_upstream-src/include' -print -quit 2>/dev/null)"
SPECBLEACH_INC="$(find "$ROOT" -maxdepth 4 -type d -path '*/_deps/libspecbleach_upstream-src/include' -print -quit 2>/dev/null)"

compile() {
    # NEREUSSDR_VERSION setzt sonst CMake. Ohne sie scheitert jede Datei,
    # die QStringLiteral(NEREUSSDR_VERSION) benutzt, mit "expected ')'" --
    # ein Fehler, der wie ein Syntaxfehler aussieht und keiner ist.
    #
    # Die Modulliste unten muss die REQUIRED-Komponenten aus CMakeLists.txt
    # abdecken. Multimedia, Svg und WebSockets fehlten bis 2026-08-16, und
    # weil der flache Baum je Framework nur EIN Verzeichnis verlinkt, fand
    # <QAudio> sich nicht -- jede Datei, die core/ClientPuduMonitor.h
    # erreicht (also auch MainWindow.cpp), scheiterte mit einem
    # "file not found", das wie ein Fehler im Code aussah und keiner war.
    # HAVE_WEBSOCKETS, NEREUS_GPU_SPECTRUM, and HAVE_WDSP: all three
    # unconditional in the real build (CMakeLists.txt: Qt6::WebSockets is
    # REQUIRED, so HAVE_WEBSOCKETS is always defined; NEREUS_GPU_SPECTRUM
    # defaults ON; HAVE_WDSP is gated on WDSP_FOUND, which is true whenever
    # third_party/wdsp/src/comm.h exists -- i.e. always, since WDSP is
    # vendored in-tree, not fetched).
    # Missing them here does not fail loudly -- it silently strips every
    # #ifdef HAVE_WEBSOCKETS / #ifdef NEREUS_GPU_SPECTRUM / #ifdef HAVE_WDSP
    # block, so a class body written entirely inside one (TciClient,
    # TciServer, half of SpectrumWidget, wdsp_api.h's real declarations)
    # parses as empty and every use of it downstream reports "incomplete
    # type" / "undeclared identifier" -- errors that look exactly like real
    # syntax errors and are not (HAVE_WEBSOCKETS/NEREUS_GPU_SPECTRUM found
    # 2026-08-24, checking the SunSDR/TCI spectrum-wiring change against a
    # checker run that had never seen these two files build clean;
    # HAVE_WDSP found 2026-08-26, checking a one-line TciServer.cpp fix
    # against wdsp_api.h's GetTXAMeter, a real declaration this script had
    # never actually exercised before).
    #
    # NEREUS_BUILD_TESTS: same story, found the same day. tools/run_tests.sh
    # always configures with -DNEREUS_BUILD_TESTS=ON (it's the whole point
    # of that build dir -- tests are opt-in so the normal build stays fast,
    # per CLAUDE.md), so every "...ForTest()" hook gated behind
    # #ifdef NEREUS_BUILD_TESTS in a production header (P1RadioConnection.h's
    # setBoardForTest/captureBank10ForTest, RadioModel's
    # injectConnectionForTest, etc.) is real, reachable code in the build
    # that actually runs the test suite -- just invisible to a checker that
    # never defines the macro. A real ctest run confirmed
    # tst_p1_alex_lpf_word_source.cpp compiles and mostly passes; this
    # script reported 15 "no member named ..." errors for methods that
    # exist and work, because they'd all been silently stripped.
    g++ -std=c++20 -fsyntax-only -fPIC \
        -DNEREUSSDR_VERSION='"0.0.0-syntaxcheck"' \
        -DHAVE_WEBSOCKETS -DNEREUS_GPU_SPECTRUM -DHAVE_WDSP -DNEREUS_BUILD_TESTS \
        -I"$QTINC" -I"$QTINC/QtCore" -I"$QTINC/QtGui" -I"$QTINC/QtWidgets" \
        -I"$QTINC/QtNetwork" -I"$QTINC/QtTest" -I"$QTINC/rhi" \
        -I"$QTINC/QtMultimedia" -I"$QTINC/QtSvg" -I"$QTINC/QtWebSockets" \
        ${PA_INC:+-I"$PA_INC"} -I"$WDSP_INC" \
        ${RNNOISE_INC:+-I"$RNNOISE_INC"} ${SPECBLEACH_INC:+-I"$SPECBLEACH_INC"} \
        -I"$ROOT/src" -I"$ROOT" "$1" 2>&1
}

# Errors that are the harness's own, not the code's.
real_errors() {
    grep "error:" | grep -v "qlogging.h"
}

# ── Self-test: prove the compiler complains about a broken file ──────
#
# Without this the whole script is a very elaborate `echo 0`.
SELFTEST="$(mktemp /tmp/nereus_selftest_XXXXXX.cpp)"
trap 'rm -f "$SELFTEST"' EXIT
printf 'int deliberately_broken( ;\n' > "$SELFTEST"
if [ "$(compile "$SELFTEST" | grep -c 'error:')" -eq 0 ]; then
    echo "syntax_check: SELF-TEST FAILED — the compiler did not object to" >&2
    echo "  a file containing 'int deliberately_broken( ;'." >&2
    echo "  Every result from this run would be meaningless. Refusing." >&2
    exit 2
fi

# ── The actual work ──────────────────────────────────────────────────

files=("$@")
if [ "${1:-}" = "--all-changed" ]; then
    # A while-read loop, not `mapfile`: that is a bash 4 builtin, and
    # macOS ships bash 3.2. `--all-changed` therefore failed here with
    # "mapfile: command not found" and then reported the WORD
    # "--all-changed" as a missing file — one error, exit non-zero, and
    # the whole changed set silently unchecked. Found 2026-08-15, the
    # first time the flag was used on a Mac.
    files=()
    while IFS= read -r f; do
        [ -n "$f" ] && files+=("$f")
    done < <(git diff --name-only HEAD -- '*.cpp' '*.cc')
    if [ "${#files[@]}" -eq 0 ]; then
        echo "syntax_check: nothing changed."
        exit 0
    fi
fi

if [ "${#files[@]}" -eq 0 ]; then
    echo "usage: tools/syntax_check.sh <file.cpp>... | --all-changed" >&2
    exit 2
fi

total=0
for f in "${files[@]}"; do
    [ -f "$f" ] || { echo "  ?  $f (no such file)"; total=$((total+1)); continue; }
    out="$(compile "$f" | real_errors)"
    n=$(printf '%s' "$out" | grep -c "error:" || true)
    printf '%4s  %s\n' "$n" "$f"
    if [ "$n" -gt 0 ]; then
        printf '%s\n' "$out" | head -12 | sed 's/^/        /'
        total=$((total+n))
    fi
done

echo
if [ "$total" -eq 0 ]; then
    echo "clean — and the self-test passed, so that means something."
else
    echo "$total error(s)."
fi
exit $(( total > 0 ? 1 : 0 ))
