#!/bin/bash
# =================================================================
# run.sh  (NereusSDR)
# =================================================================
#
# Build the working tree and run exactly that binary.
#
# This exists because of three separate mistakes this repository's
# development has already made, each of which cost real time:
#
#   1. Reading a binary that had not been rebuilt, and concluding a
#      change had not worked.
#   2. Launching the installed NereusSDR.app instead of the one just
#      built, and concluding the same.
#   3. Leaving the previous instance running, so the terminal swallowed
#      the next command and nothing happened at all.
#
# So: kill any running copy, build, print which commit is about to run,
# and launch by explicit path. Never `open -a Longpath`, which finds
# whichever copy Launch Services prefers.
#
# =================================================================
# Modification history (NereusSDR):
#   2026-08-07 — Created for NereusSDR, AI-assisted via Anthropic
#                 Claude (Cowork), operator Martin Fischer.
# =================================================================

set -euo pipefail

cd "$(dirname "$0")"

BIN="./build/Longpath.app/Contents/MacOS/Longpath"

# An old instance holds the terminal and, worse, looks like the new one.
pkill -x Longpath 2>/dev/null || true

if [ ! -d build ]; then
    echo "No build directory — configuring first."
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
fi

# Quiet unless something is wrong: the interesting output is the errors,
# and a wall of shader lines trains you to stop reading.
if ! cmake --build build -j"$(sysctl -n hw.ncpu)" > /tmp/nereus-build.log 2>&1; then
    echo "── build failed ──────────────────────────────────────────"
    tail -40 /tmp/nereus-build.log
    echo "──────────────────────────────────────────────────────────"
    echo "Full log: /tmp/nereus-build.log"
    exit 1
fi

# Warnings are worth seeing even when the build succeeds; they are the
# ones that turn into the next bug.
if grep -q "warning:" /tmp/nereus-build.log; then
    echo "Build warnings:"
    grep "warning:" /tmp/nereus-build.log | sort -u | head -20
    echo
fi

echo "Running $(git rev-parse --abbrev-ref HEAD)@$(git rev-parse --short HEAD)"
if [ -n "$(git status --porcelain)" ]; then
    echo "  (working tree has uncommitted changes — they ARE in this build)"
fi
echo

exec "$BIN" "$@"
