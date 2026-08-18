#!/usr/bin/env python3
"""Die Bremse gegen die Stil-Drift.

WARUM ES DAS GIBT. Die Palette wurde zweimal aufgeraeumt — am
2026-08-15 von 276 auf 202 Farben, am 2026-08-18 von 297 auf 284. Der
Zwischenstand sagt alles: zwischen den beiden Aufraeumrunden ist die
Zahl von 202 auf 297 GEWACHSEN. Jede neu gebaute Flaeche hat wieder
namenlose Farben und rohe Schriftgroessen mitgebracht.

Ein Aufraeumen ohne Bremse ist darum keine Verbesserung, sondern eine
wiederkehrende Ausgabe. Diese Pruefung ist die Bremse.

WIE SIE ARBEITET — als RATSCHE, nicht als Verbot. Ein Verbot waere
heute nicht erfuellbar (243 namenlose Farben, 514 rohe Schriftgroessen)
und wuerde darum sofort abgeschaltet. Die Ratsche nimmt den heutigen
Stand als Decke:

  * waechst eine Zahl  -> FEHLER, mit der Nennung was dazukam
  * faellt eine Zahl   -> Hinweis, die Decke gehoert nachgezogen
  * bleibt sie gleich  -> still

Damit kostet Aufraeumen nichts und Nachlassen faellt auf. Die Decke
steht in docs/design/style-drift-baseline.json und wird mit
--update-baseline nachgezogen, NUR nach unten.

Benutzung:
    python3 scripts/verify-style-drift.py                 # pruefen
    python3 scripts/verify-style-drift.py --verbose       # mit Fundstellen
    python3 scripts/verify-style-drift.py --update-baseline
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BASELINE = REPO / "docs" / "design" / "style-drift-baseline.json"

# Die Schriftleiter aus StyleConstants.h. Eine Groesse, die NICHT darauf
# liegt, ist Drift: sie ist gewachsen, nicht entschieden.
LADDER = {9, 11, 13, 16, 22, 38}
RE_FONT_PX = re.compile(r"font-size:\s*(\d+)px")
RE_SET_PIXEL = re.compile(r"setPixelSize\(\s*(\d+)\s*\)")


def _load_audit():
    """tools/colour_audit.py als Modul laden.

    Die Logik dort ist die Wahrheit ueber Farbliterale — sie hier ein
    zweites Mal zu schreiben hiesse, zwei Zaehlungen zu pflegen, die
    irgendwann auseinanderlaufen und dann beide unglaubwuerdig sind.
    """
    spec = importlib.util.spec_from_file_location(
        "colour_audit", REPO / "tools" / "colour_audit.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def measure() -> tuple[dict, dict]:
    """Zaehlen. Liefert (Zahlen, Fundstellen)."""
    audit = _load_audit()
    palette = audit.named_palette()
    lits = audit.literals()

    # classify() liefert (Name, Wert, ΔE) des NAECHSTEN benannten Tons,
    # nicht einen Eimer. „Benannt" heisst darum: der Wert steht selbst
    # in der Palette — nicht, dass classify irgendetwas gefunden hat.
    #
    # Beim ersten Versuch habe ich `bucket != "named"` geprueft und
    # damit 284 statt 243 gezaehlt, also JEDE Farbe als namenlos. Eine
    # Bremse, die zu hoch zaehlt, laesst echtes Wachstum durch, solange
    # es unter ihrer falschen Decke bleibt.
    palette_values = {v.lower() for v in palette.values()}
    unnamed = {c for c in lits if c.lower() not in palette_values}

    font_hits: list[str] = []
    for path in sorted(REPO.glob("src/**/*")):
        if path.suffix not in (".cpp", ".h", ".hpp", ".cc"):
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except Exception:
            continue
        rel = path.relative_to(REPO)
        for rx in (RE_FONT_PX, RE_SET_PIXEL):
            for m in rx.finditer(text):
                px = int(m.group(1))
                if px not in LADDER:
                    line = text[:m.start()].count("\n") + 1
                    font_hits.append(f"{rel}:{line}  {m.group(0)}")

    counts = {
        "unnamed_colours": len(unnamed),
        "colour_literals": sum(len(v) for v in lits.values()),
        "off_ladder_font_sizes": len(font_hits),
    }
    return counts, {"off_ladder": font_hits}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--update-baseline", action="store_true",
                    help="Decke nachziehen (nur nach unten)")
    ap.add_argument("--verbose", action="store_true",
                    help="Fundstellen nennen")
    args = ap.parse_args()

    counts, detail = measure()

    if not BASELINE.is_file():
        BASELINE.parent.mkdir(parents=True, exist_ok=True)
        BASELINE.write_text(json.dumps(counts, indent=2) + "\n",
                            encoding="utf-8")
        print(f"[style-drift] Decke neu angelegt: {BASELINE.name}")
        for k, v in counts.items():
            print(f"    {k:26s} {v}")
        return 0

    base = json.loads(BASELINE.read_text(encoding="utf-8"))
    grown, shrunk = [], []
    for key, now in counts.items():
        was = base.get(key)
        if was is None:
            grown.append((key, "neu", now))
        elif now > was:
            grown.append((key, was, now))
        elif now < was:
            shrunk.append((key, was, now))

    if args.update_baseline:
        if grown:
            print("[style-drift] Decke NICHT nachgezogen — es ist "
                  "gewachsen, nicht gefallen:", file=sys.stderr)
            for key, was, now in grown:
                print(f"    {key}: {was} -> {now}", file=sys.stderr)
            return 1
        BASELINE.write_text(json.dumps(counts, indent=2) + "\n",
                            encoding="utf-8")
        print("[style-drift] Decke nachgezogen:")
        for key, was, now in shrunk:
            print(f"    {key}: {was} -> {now}")
        if not shrunk:
            print("    (unveraendert)")
        return 0

    print("[style-drift]")
    for key, now in counts.items():
        was = base.get(key, "?")
        mark = "  " if now == was else ("^^" if isinstance(was, int)
                                        and now > was else "vv")
        print(f"  {mark} {key:26s} {now:5d}   Decke {was}")

    if args.verbose and detail["off_ladder"]:
        print("\n  Schriftgroessen ausserhalb der Leiter "
              f"({sorted(LADDER)}):")
        for line in detail["off_ladder"][:40]:
            print(f"    {line}")
        if len(detail["off_ladder"]) > 40:
            print(f"    ... und {len(detail['off_ladder']) - 40} weitere")

    if grown:
        print("\n[style-drift] FEHLER — die Drift ist gewachsen:",
              file=sys.stderr)
        for key, was, now in grown:
            print(f"    {key}: {was} -> {now}", file=sys.stderr)
        print("\n  Eine neue namenlose Farbe oder Schriftgroesse ist "
              "keine Kleinigkeit: sie folgt keinem Palettenwechsel und "
              "wird beim naechsten Aufraeumen wieder Arbeit. Entweder "
              "einen Namen in StyleConstants.h geben oder eine "
              "vorhandene Stufe nehmen.", file=sys.stderr)
        return 1

    if shrunk:
        print("\n  Gefallen — Decke nachziehen mit --update-baseline:")
        for key, was, now in shrunk:
            print(f"    {key}: {was} -> {now}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
