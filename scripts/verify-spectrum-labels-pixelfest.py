#!/usr/bin/env python3
"""Fail if a panadapter/waterfall label uses a DPI-dependent font size.

Betreiber am 2026-09-24: "die fontgroesse des graphen sollte immer die
gleiche pixelgroesse haben" -- beim Panadapter/Wasserfall (SpectrumWidget)
wurde die Groesse an den meisten Stellen ueber setPixelSize() gesetzt
(Hausstil, siehe capsFont() in StyleConstants.h -- Beschriftungen auf
Graphen bleiben pixelfest, unabhaengig von der System-DPI), an sechs
Stellen aber ueber das DPI-abhaengige setPointSize()/setPointSizeF():
dBm-Skala, Bandplan-Zeile, Wasserfall-"LIVE"-Chip, Wasserfall-
Zeitachse, IMD-Overlay und der "HIGH SWR"-Warntext. Auf Bildschirmen
mit unterschiedlicher Skalierung wich diese Schrift von der uebrigen
Panadapter-Beschriftung ab.

Bewusst NUR auf SpectrumWidget.cpp beschraenkt: das ist der eine
Graph, um den es ging. Andere Grafiken im Baum (Kanalstreifen-Meter,
EQ-Kurve, Verstaerker-Diagramm, Setup-Protokoll) sind eigene Widgets
mit eigenem Stil und nicht Teil dieser Meldung -- ein Sammelumbau ueber
den ganzen Baum ist am 2026-09-23/24 schon einmal am falschen Umfang
gescheitert (siehe scripts/verify-no-native-color-dialog.py's
Entstehungsgeschichte).
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TARGET = ROOT / "src" / "gui" / "SpectrumWidget.cpp"

PATTERN = re.compile(r"\.setPointSize(F)?\s*\(")


def main() -> int:
    if not TARGET.exists():
        print(f"[spectrum-labels-pixelfest] {TARGET} nicht gefunden -- nichts zu pruefen.")
        return 0

    failures = []
    for num, line in enumerate(TARGET.read_text(encoding="utf-8").splitlines(), 1):
        stripped = line.strip()
        if stripped.startswith("//") or stripped.startswith("*"):
            continue
        if PATTERN.search(line):
            failures.append(f"{num}: {stripped}")

    if failures:
        rel = TARGET.relative_to(ROOT).as_posix()
        print(f"[spectrum-labels-pixelfest] {rel} setzt eine Schriftgroesse "
              "DPI-abhaengig (setPointSize/setPointSizeF):")
        for f in failures:
            print(f"  {f}")
        print()
        print("Der Panadapter/Wasserfall haelt seine Beschriftungen pixelfest")
        print("(setPixelSize) -- siehe capsFont() in StyleConstants.h. Verwende")
        print("setPixelSize() mit demselben Zahlenwert statt setPointSize().")
        return 1

    print("[spectrum-labels-pixelfest] OK: SpectrumWidget.cpp setzt keine "
          "Schriftgroesse DPI-abhaengig.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
