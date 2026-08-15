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
# ── Wie lange es dauern muss ──────────────────────────────────────────
#
# 2026-08-15, OE5SOS: „müssen immer alle über 600 Zeilen durchlaufen
# werden, dauert sehr lange". Nein — und es waren zwei Bremsen drin:
#
#   · ctest lief OHNE -j. 658 Tests nacheinander, obwohl der Rechner
#     acht Kerne hat. Das Bauen war schon parallel, das Laufen nicht.
#
#   · Mit Filter wurden trotzdem ALLE sechzig Testprogramme gebaut. Bei
#     einer Ein-Datei-Änderung war das Bauen dessen, was gar nicht laufen
#     sollte, der größere Teil der Wartezeit.
#
# Beides behoben. Ein gefilterter Lauf baut jetzt nur die passenden
# Programme und lässt sie parallel laufen.
#
# Usage:
#   tools/run_tests.sh              alles bauen und laufen lassen
#   tools/run_tests.sh theme        nur Tests, die auf "theme" passen
#   tools/run_tests.sh 'swr|curve'  Regex geht auch
#   tools/run_tests.sh -L gui       nur ein Label: core, gui, models
#   tools/run_tests.sh --failed     nur die, die zuletzt gescheitert sind
#   tools/run_tests.sh --serial     nacheinander (wenn GUI-Tests zicken)
#   tools/run_tests.sh -- --verbose Rest an ctest durchreichen
#
# Modification history (NereusSDR):
#   2026-08-08 — Created for NereusSDR by Martin Fischer, AI-assisted
#                 via Anthropic Claude (Cowork).
#   2026-08-15 — Parallel laufen lassen; mit Filter nur die passenden
#                 Programme bauen; --failed, --serial, -L ergänzt.

set -u -o pipefail

cd "$(dirname "$0")/.." || exit 1
BUILD=build-tests

FILTER=""
LABEL=""
SERIAL=0
RERUN=0
EXTRA=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --)       shift; EXTRA+=("$@"); break ;;
        --serial) SERIAL=1; shift ;;
        --failed) RERUN=1; shift ;;
        -L)       LABEL="${2:-}"; shift 2 ;;
        -*)       EXTRA+=("$1"); shift ;;
        *)        if [[ -z "$FILTER" ]]; then FILTER="$1"; else EXTRA+=("$1"); fi
                  shift ;;
    esac
done

JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"
# GUI-Tests machen Fenster auf. Acht davon gleichzeitig ist auf macOS in
# aller Regel unproblematisch, aber wenn einer wegen Fokus oder
# Fensterexposition zickt, ist --serial die Antwort — und dann ist es
# eine Flake und kein Fehler, was zu wissen mehr wert ist als die Zeit.
TEST_JOBS="${NEREUS_TEST_JOBS:-$JOBS}"
[[ $SERIAL -eq 1 ]] && TEST_JOBS=1

echo "── configure ─────────────────────────────────────────────────"
cmake -S . -B "$BUILD" -G Ninja -DNEREUS_BUILD_TESTS=ON || exit 1

echo
echo "── build ─────────────────────────────────────────────────────"
# ── Nur bauen, was auch laufen soll ──────────────────────────────────
#
# Mit einem Filter wurden vorher trotzdem alle sechzig Testprogramme
# gebaut. Bei einer Ein-Datei-Änderung war das Kompilieren dessen, was
# gar nicht laufen sollte, der größere Teil der Wartezeit.
#
# Die Zielnamen stehen in tests/CMakeLists.txt als nereus_add_test(...).
TARGETS=()
if [[ -n "$FILTER" ]]; then
    while IFS= read -r t; do
        [[ -n "$t" ]] && TARGETS+=("$t")
    done < <(grep -oE 'nereus_add_test\([A-Za-z0-9_]+\)' tests/CMakeLists.txt \
             | sed -E 's/nereus_add_test\((.*)\)/\1/' \
             | grep -E "$FILTER" || true)
fi

if [[ ${#TARGETS[@]} -gt 0 ]]; then
    echo "Filter \"$FILTER\" trifft ${#TARGETS[@]} Testprogramm(e):"
    printf '  %s\n' "${TARGETS[@]}"
    echo
    cmake --build "$BUILD" --target "${TARGETS[@]}" -j "$JOBS" -- -k 0
    BUILD_RC=$?
else
    if [[ -n "$FILTER" ]]; then
        echo "Kein Testprogramm passt auf \"$FILTER\" — es wird alles"
        echo "gebaut, damit ctest den Filter selbst anwenden kann."
        echo
    fi
    # Keep going after a failure rather than stopping at the first one: on a
    # first run the useful information is HOW MANY tests fail to build and
    # which, not the name of the alphabetically earliest.
    cmake --build "$BUILD" --target all_tests -j "$JOBS" -- -k 0
    BUILD_RC=$?
fi
if [[ $BUILD_RC -ne 0 ]]; then
    echo
    echo "Some tests did not build. Running the ones that did."
fi

echo
echo "── run ───────────────────────────────────────────────────────"
CT=(ctest --test-dir "$BUILD" --output-on-failure -j "$TEST_JOBS")
[[ -n "$FILTER" ]] && CT+=(-R "$FILTER")
[[ -n "$LABEL"  ]] && CT+=(-L "$LABEL")
[[ $RERUN -eq 1 ]] && CT+=(--rerun-failed)
[[ $TEST_JOBS -gt 1 ]] && echo "($TEST_JOBS parallel — bei Zicken: --serial)" && echo
"${CT[@]}" "${EXTRA[@]+"${EXTRA[@]}"}"
CTEST_RC=$?

echo
if [[ $BUILD_RC -ne 0 ]]; then
    echo "Build had failures — the run above is only the subset that compiled."
fi
exit $(( CTEST_RC != 0 ? CTEST_RC : BUILD_RC ))
