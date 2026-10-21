#!/usr/bin/env python3
"""Jede Pruefstandsquelle muss in tests/CMakeLists.txt angemeldet sein.

Warum dieses Skript existiert
-----------------------------
`tests/CMakeLists.txt` wird NICHT geglobbt (CLAUDE.local.md: „Neue
Dateien von Hand eintragen ... eine nicht eingetragene Datei wird
stillschweigend nicht uebersetzt").  Das ist Absicht, hat aber einen
Preis: wer den Eintrag verliert, verliert den Prueflauf lautlos.  Kein
rotes Licht, kein uebersprungener Test -- er existiert einfach nicht
mehr.

Zweimal passiert, beide Male durch einen Squash-Konflikt in genau
dieser Datei:

* `tst_rtty_senden` -- gefunden und wieder angemeldet in #262.
* `tst_palette_faerbt_das_fenster`, `tst_stationsrufzeichen`,
  `tst_suche_blockiert_den_start`, `tst_tx_bandbreiten_klickbar` --
  alle vier von c0f962fb (#254) mit entfernt, am 2026-10-21 gefunden.
  Fuenf Tage unbemerkt; `tst_suche_blockiert_den_start` ist der
  Waechter fuer einen GEMESSENEN Startblocker (4656 ms -> 1 ms, #251).

Plattformtore stoeren nicht: ein `longpath_add_test(...)` innerhalb
eines `if(UNIX AND NOT APPLE)` steht trotzdem in der Datei.  Geprueft
wird die Anmeldung, nicht ob der Test auf dieser Maschine laeuft.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
TESTS = REPO / "tests"
CMAKE = TESTS / "CMakeLists.txt"

# Quellen, die absichtlich keinen eigenen Prueflauf bilden.  Jede Zeile
# braucht einen Grund; eine leere Ausnahmeliste ist das Ziel.
AUSNAHMEN: dict[str, str] = {}


def main() -> int:
    if not CMAKE.is_file():
        print(f"FATAL: {CMAKE} fehlt", file=sys.stderr)
        return 2

    text = CMAKE.read_text(encoding="utf-8")
    angemeldet = set(re.findall(r"longpath_add_test\(\s*([A-Za-z0-9_]+)", text))

    quellen = sorted(p.stem for p in TESTS.glob("tst_*.cpp"))
    if not quellen:
        print("FATAL: keine tests/tst_*.cpp gefunden", file=sys.stderr)
        return 2

    fehlend = [q for q in quellen if q not in angemeldet and q not in AUSNAHMEN]

    # Gegenrichtung: ein Eintrag ohne Quelle laesst cmake schon beim
    # Konfigurieren scheitern, darum hier nur als Hinweis.
    ohne_quelle = sorted(a for a in angemeldet if not (TESTS / f"{a}.cpp").is_file())

    print(f"{len(quellen)} Pruefstandsquellen, {len(angemeldet)} Anmeldungen")

    if ohne_quelle:
        print(f"Hinweis: {len(ohne_quelle)} Anmeldung(en) ohne tst_*.cpp "
              f"(eigene Quelldatei-Namen sind erlaubt): "
              f"{', '.join(ohne_quelle[:5])}"
              + (" ..." if len(ohne_quelle) > 5 else ""))

    if fehlend:
        print()
        print(f"FEHLER: {len(fehlend)} Pruefstandsquelle(n) sind NICHT in "
              f"tests/CMakeLists.txt angemeldet und werden damit nie "
              f"uebersetzt und nie gefahren:", file=sys.stderr)
        for f in fehlend:
            print(f"  tests/{f}.cpp", file=sys.stderr)
        print(file=sys.stderr)
        print("Eintragen mit `longpath_add_test(<name>)` -- oder, wenn die "
              "Datei absichtlich keinen eigenen Lauf bildet, mit Grund in "
              "AUSNAHMEN dieses Skripts.", file=sys.stderr)
        return 1

    print("alle angemeldet")
    return 0


if __name__ == "__main__":
    sys.exit(main())
