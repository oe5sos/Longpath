#!/bin/bash
# =================================================================
# build.sh  (NereusSDR)
# =================================================================
#
# Build, and do nothing else.
#
# ── Why this exists, and it is not a nice-to-have ────────────────────
#
# run.sh ends with `exec "$BIN"` — the app REPLACES the shell. That is
# deliberate and right for running, and it makes run.sh unusable for
# finding out whether the code compiles:
#
#   * the prompt never returns until the app is quit;
#   * `./run.sh | tail -40` prints NOTHING while the app runs, because
#     tail buffers until the pipe closes;
#   * anything typed meanwhile queues up invisibly and fires later.
#
# On 2026-08-11 that cost the better part of an hour. Seven commands
# went into a terminal that had never returned a prompt, the screen
# stayed blank, and the build was reported as broken when it was
# merely slow. It had happened once before in the same project, and
# run.sh's own header lists it as reason 3 for run.sh existing.
#
# A script whose failure mode is silence is the fault, not the user.
# So: this one builds, prints what happened, and exits. Always.
#
# Usage:  ./build.sh          build, show errors and warnings
#         ./build.sh --quiet  exit status only, for scripts
#
# =================================================================
# Modification history (NereusSDR):
#   2026-08-11 — Created for NereusSDR, AI-assisted via Anthropic
#                 Claude (Cowork), operator Martin Fischer.
# =================================================================

set -uo pipefail

cd "$(dirname "$0")"

LOG=/tmp/nereus-build.log
QUIET=0
[ "${1:-}" = "--quiet" ] && QUIET=1

if [ ! -d build ]; then
    echo "No build directory — configuring first."
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" || exit 2
fi

# Progress on screen AND the full text in the log. run.sh hides the
# output to keep a successful run quiet; here the whole point is to
# watch, so a full rebuild does not look like a hang.
START=$(date +%s)
if [ "$QUIET" = "1" ]; then
    cmake --build build -j"$(sysctl -n hw.ncpu)" > "$LOG" 2>&1
    STATUS=$?
else
    cmake --build build -j"$(sysctl -n hw.ncpu)" 2>&1 | tee "$LOG"
    STATUS=${PIPESTATUS[0]}
fi
TOOK=$(( $(date +%s) - START ))

echo
if [ "$STATUS" -ne 0 ]; then
    echo "── build FAILED after ${TOOK}s ───────────────────────────"
    grep -E "error:|FAILED" "$LOG" | head -30
    echo "──────────────────────────────────────────────────────────"
    echo "Full log: $LOG"
    exit 1
fi

if grep -q "warning:" "$LOG"; then
    echo "Warnings:"
    grep "warning:" "$LOG" | sort -u | head -20
    echo
fi

echo "Build OK in ${TOOK}s — $(git rev-parse --abbrev-ref HEAD)@$(git rev-parse --short HEAD)"
if [ -n "$(git status --porcelain)" ]; then
    echo "  (uncommitted changes are in this build)"
fi
echo "Run it with ./run.sh"
