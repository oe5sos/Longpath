#!/bin/sh
# =================================================================
# scripts/make-app-icons.sh  (Longpath)
# =================================================================
#
# Erzeugt aus EINER SVG-Quelle alle Programmsymbole, damit sie nicht
# auseinanderlaufen. Quelle: resources/branding/longpath-mark.svg
#
# Aufruf aus dem Wurzelverzeichnis:  sh scripts/make-app-icons.sh
#
# Nur macOS: qlmanage und iconutil sind Apple-Werkzeuge. Fuer Windows
# (.ico) und Linux fehlt der Weg noch — dort bleibt das vorhandene
# .ico stehen, bis jemand ImageMagick oder rsvg-convert einrichtet.
#
# Marke: „Das O ist die Erde", vom Betreiber gewaehlt am 2026-08-20.
# =================================================================

set -e
SRC=resources/branding/longpath-mark.svg
OUT=resources/icons
TMP=$(mktemp -d)

# Aus der SVG-Quelle in PNG. qlmanage kann SVG auf macOS, rsvg-convert
# waere schoener, ist aber nicht installiert. Deshalb der Umweg ueber
# eine grosse PNG und sips zum Verkleinern — sips rechnet sauber
# herunter, aber nicht aus SVG herauf.
qlmanage -t -s 1024 -o "$TMP" "$SRC" >/dev/null 2>&1
BIG="$TMP/$(basename $SRC).png"
[ -f "$BIG" ] || { echo "qlmanage hat nichts erzeugt"; exit 1; }

mkdir -p "$OUT/Longpath.iconset"
for s in 16 32 64 128 256 512 1024; do
  sips -z $s $s "$BIG" --out "$TMP/icon_${s}.png" >/dev/null
done
cp "$TMP/icon_16.png"   "$OUT/Longpath.iconset/icon_16x16.png"
cp "$TMP/icon_32.png"   "$OUT/Longpath.iconset/icon_16x16@2x.png"
cp "$TMP/icon_32.png"   "$OUT/Longpath.iconset/icon_32x32.png"
cp "$TMP/icon_64.png"   "$OUT/Longpath.iconset/icon_32x32@2x.png"
cp "$TMP/icon_128.png"  "$OUT/Longpath.iconset/icon_128x128.png"
cp "$TMP/icon_256.png"  "$OUT/Longpath.iconset/icon_128x128@2x.png"
cp "$TMP/icon_256.png"  "$OUT/Longpath.iconset/icon_256x256.png"
cp "$TMP/icon_512.png"  "$OUT/Longpath.iconset/icon_256x256@2x.png"
cp "$TMP/icon_512.png"  "$OUT/Longpath.iconset/icon_512x512.png"
cp "$TMP/icon_1024.png" "$OUT/Longpath.iconset/icon_512x512@2x.png"

iconutil -c icns "$OUT/Longpath.iconset" -o "$OUT/Longpath.icns"
cp "$TMP/icon_512.png" "$OUT/Longpath.png"
echo "icns + png erzeugt"
rm -rf "$TMP"
