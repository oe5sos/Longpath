#!/usr/bin/env bash
#
# Build and run the whole test suite.
#
# It has never been run. Around 290 test cases exist across some fifty
# executables, every one of them written against real behaviour and
# checked only for syntax — which means the suite's own correctness is
# unverified, and the first run should be expected to fail somewhere.
# That is not a reason to delay it. A test that has never executed is a
# comment with punctuation.
#
# Tests are opt-in (-DNEREUS_BUILD_TESTS=ON) so the normal build stays
# fast; this script turns them on in a separate build directory so it
# cannot disturb the one ./run.sh uses.
#
# Usage:
#   tools/run_tests.sh              build and run everything
#   tools/run_tests.sh adif         run only tests matching "adif"
#   tools/run_tests.sh -- --verbose pass the rest through to ctest
#
# Modification history (NereusSDR):
#   2026-08-08 — Created for NereusSDR by Martin Fischer, AI-assisted
#                 via Anthropic Claude (Cowork).

set -u -o pipefail

cd "$(dirname "$0")/.." || exit 1
BUILD=build-tests

FILTER=""
EXTRA=()
if [[ $# -gt 0 ]]; then
    if [[ "$1" == "--" ]]; then shift; EXTRA=("$@")
    else FILTER="$1"; shift; EXTRA=("$@")
    fi
fi

JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"

echo "── configure ─────────────────────────────────────────────────"
cmake -S . -B "$BUILD" -G Ninja -DNEREUS_BUILD_TESTS=ON || exit 1

echo
echo "── build ─────────────────────────────────────────────────────"
# Keep going after a failure rather than stopping at the first one: on a
# first run the useful information is HOW MANY tests fail to build and
# which, not the name of the alphabetically earliest.
cmake --build "$BUILD" --target all_tests -j "$JOBS" -- -k 0
BUILD_RC=$?
if [[ $BUILD_RC -ne 0 ]]; then
    echo
    echo "Some tests did not build. Running the ones that did."
fi

echo
echo "── run ───────────────────────────────────────────────────────"
if [[ -n "$FILTER" ]]; then
    ctest --test-dir "$BUILD" -R "$FILTER" --output-on-failure "${EXTRA[@]+"${EXTRA[@]}"}"
else
    ctest --test-dir "$BUILD" --output-on-failure "${EXTRA[@]+"${EXTRA[@]}"}"
fi
CTEST_RC=$?

echo
if [[ $BUILD_RC -ne 0 ]]; then
    echo "Build had failures — the run above is only the subset that compiled."
fi
exit $(( CTEST_RC != 0 ? CTEST_RC : BUILD_RC ))
