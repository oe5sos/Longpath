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

set -u -o pipefail

cd "$(dirname "$0")/.." || exit 2
ROOT="$PWD"

# Where the Qt headers were checked out. Overridable, because this path
# is a property of one machine and the script is not.
QTINC="${NEREUS_QTINC:-/sessions/relaxed-zealous-mendel/tmp/qtinc}"

if [ ! -d "$QTINC/QtCore" ]; then
    echo "syntax_check: no Qt headers at $QTINC" >&2
    echo "  set NEREUS_QTINC to a qtbase include tree." >&2
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
    mapfile -t files < <(git diff --name-only HEAD -- '*.cpp' '*.cc')
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
