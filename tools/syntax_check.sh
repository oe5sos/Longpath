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
# Modification history (NereusSDR):
#   2026-08-10 — Created for NereusSDR by Martin Fischer, AI-assisted
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
        # does not, which is why this is conditional rather than an
        # unconditional link: a dangling one would put an -I on the
        # command line that silently resolves nothing, and the first
        # symptom would be a confusing error in a file that includes
        # <rhi/qrhi.h>.
        if [ -d "$libdir/QtGui.framework/Headers/rhi" ]; then
            ln -sfn "$libdir/QtGui.framework/Headers/rhi" "$shim/rhi"
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

compile() {
    g++ -std=c++20 -fsyntax-only -fPIC \
        -I"$QTINC" -I"$QTINC/QtCore" -I"$QTINC/QtGui" -I"$QTINC/QtWidgets" \
        -I"$QTINC/QtNetwork" -I"$QTINC/QtTest" -I"$QTINC/rhi" \
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
