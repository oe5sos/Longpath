#!/bin/bash
# =================================================================
# packaging/macos/check-min-macos.sh  (Longpath)
# =================================================================
#
# Longpath-original.
#
# Prueft ein fertiges Longpath.app, bevor es signiert und verpackt wird:
#
#   1. Keine Mach-O-Datei im Paket verlangt ein neueres macOS als die
#      Untergrenze (Aufruf: check-min-macos.sh <App> <Untergrenze> [<Arch>]).
#      dyld bricht auf einem aelteren macOS beim Laden der ersten zu
#      neuen Bibliothek ab -- Longpath startet dann gar nicht.
#   2. Jede Mach-O-Datei enthaelt die erwartete Architektur.
#   3. Keine Mach-O-Datei verweist nach aussen: jede Bibliothek und jeder
#      Suchpfad (LC_RPATH) liegt im Paket (@executable_path, @loader_path,
#      @rpath) oder im System (/System, /usr/lib). 0.6.4 Intel trug
#      „/opt/homebrew/lib" als ERSTEN Suchpfad -- auf einem Mac mit
#      Homebrew-Qt laedt so ein Paket dessen Qt statt des eigenen, und mit
#      Qt 6.8 im Paket startete es dann gar nicht (2026-09-29 nachgestellt).
#
# Warum es das gibt (2026-09-29): die Apple-Silicon-Fassung 0.6.4 trug
# „deployment_target 14.0", enthielt aber Qt und FFTW aus Homebrew mit
# minos 15.0 -- auf macOS 12, 13 und 14 startete sie nicht, und niemand
# merkte es beim Bauen. Die Intel-Fassung trug „11.0" und Qt mit 12.0.
#
# Beide Kennungen werden gelesen: LC_BUILD_VERSION (minos, heutige
# Werkzeuge) und LC_VERSION_MIN_MACOSX (version, aeltere Dateien).
#
# Modification history (Longpath):
#   2026-09-29 — Original fuer Longpath von Martin Fischer,
#                 KI-gestuetzt ueber Anthropic Claude.
# =================================================================
set -uo pipefail

APP="${1:?Aufruf: $0 <Longpath.app> <Untergrenze, z. B. 12.0> [<Arch>]}"
FLOOR="${2:?Untergrenze fehlt}"
ARCH="${3:-}"

newer_than() {   # $1 > $2 (Versionsvergleich)
    [ "$1" != "$2" ] && [ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | tail -1)" = "$1" ]
}

bad=0
checked=0
highest="0"
while IFS= read -r -d '' f; do
    file -b "$f" | grep -q "Mach-O" || continue
    checked=$((checked + 1))
    # Hoechste Untergrenze ueber alle Architektur-Scheiben der Datei.
    mins=$(otool -l "$f" 2>/dev/null | awk '
        /cmd LC_BUILD_VERSION/      {b=1; v=0; next}
        /cmd LC_VERSION_MIN_MACOSX/ {v=1; b=0; next}
        /cmd /                      {b=0; v=0}
        b && $1=="minos"            {print $2}
        v && $1=="version"          {print $2}')
    m=$(printf '%s\n' $mins | sort -V | tail -1)
    [ -z "$m" ] && continue
    newer_than "$m" "$highest" && highest="$m"
    rel="${f#"$APP"/}"
    if newer_than "$m" "$FLOOR"; then
        echo "::error::$rel verlangt macOS $m (Untergrenze $FLOOR)"
        bad=1
    fi
    outside=$( { otool -L "$f" 2>/dev/null | tail -n +2 | awk '{print $1}';
                 otool -l "$f" 2>/dev/null | awk '/cmd LC_RPATH/{r=1; next} r && $1=="path"{print $2; r=0}'; } \
               | grep -v -E '^(@executable_path|@loader_path|@rpath|/System/|/usr/lib/)' \
               | grep -v -x "$f" | sort -u | tr '\n' ' ')
    if [ -n "$outside" ]; then
        echo "::error::$rel verweist aus dem Paket hinaus: $outside"
        bad=1
    fi
    if [ -n "$ARCH" ] && ! lipo -archs "$f" 2>/dev/null | tr ' ' '\n' | grep -qx "$ARCH"; then
        echo "::error::$rel enthaelt kein $ARCH (nur: $(lipo -archs "$f" 2>/dev/null))"
        bad=1
    fi
done < <(find "$APP" -type f -print0)

echo "$checked Mach-O-Dateien geprueft, hoechste Untergrenze: macOS $highest (erlaubt: $FLOOR)"
if [ "$checked" -eq 0 ]; then
    echo "::error::keine Mach-O-Datei in $APP gefunden -- Pruefung kaputt?"
    exit 1
fi
exit $bad
