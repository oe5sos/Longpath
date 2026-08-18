#!/usr/bin/env python3
"""Farbinventur für NereusSDR.

Warum das Skript existiert
--------------------------

Am 2026-08-15 sollte die Palette Richtung Zeus verschoben werden. Der
erste Schritt schien mechanisch: Hex-Literale durch die Konstanten aus
``src/gui/StyleConstants.h`` ersetzen. Geschätzt waren dreißig Dateien.

Gezählt waren es 1733 Literale in 130 Dateien — und, was mehr wiegt:
**276 verschiedene Farben, von denen 241 keinen Namen haben.** 78
verschiedene Blautöne, 40 Grüntöne, 40 Rottöne. Das ist keine Palette,
das ist Drift: jeder hat die Farbe hingeschrieben, die gerade passend
aussah.

Eine Palette mit 276 Farben lässt sich nicht umstellen. Sie muss erst
eingedampft werden, und dafür braucht es eine Zahl statt eines Gefühls.

Was es misst
------------

Für jede namenlose Farbe den Abstand zur nächsten benannten, in CIE-Lab
(ΔE76 — grob genug für Chrome-Farben, und der Unterschied zu ΔE2000
ändert an der Einteilung nichts):

    ΔE < 8     nicht unterscheidbar → ersetzbar, keine Entscheidung
    ΔE 8–18    sichtbar, aber nah → eine Entscheidung pro Farbe
    ΔE > 18    eigene Farbe → braucht einen Namen oder muss weg

Aufruf
------

    python3 tools/colour_audit.py                # Übersicht
    python3 tools/colour_audit.py --collapse     # die ersetzbaren, mit Fundort
    python3 tools/colour_audit.py --orphans      # die eigenständigen
    python3 tools/colour_audit.py --unmapped     # die, die kein Theme erreicht
    python3 tools/colour_audit.py --apply        # Schritt 1 ausführen

Bis auf ``--apply`` liest das Skript nur.

Was ``--apply`` macht — und was nicht
-------------------------------------

Es ersetzt **Wert durch Wert**, nicht Wert durch Konstante:
``#404858`` wird zu ``#3a4a5a``. Das ist reine Textersetzung und kann
den Build nicht brechen — anders als der Umbau eines Stylesheet-Strings
in eine ``.arg()``-Kette, der bei 1141 Vorkommen nicht blind machbar ist.

Danach greift ``Style::themed()``: die Farbe hat jetzt einen Wert, den
die Theme-Tabelle kennt, also folgt sie dem Palettenwechsel.

Nur der Eimer ΔE < 8 wird angefasst. Diese Farben sind von ihrem Ziel
nicht zu unterscheiden — ``#404858`` steht 18-mal in der App und liegt
2,9 ΔE neben ``kTitleGradTop``. Es geht keine Information verloren,
weil da keine war.

Was ``--unmapped`` meldet — der Fehlertyp hinter #00e5ff
-------------------------------------------------------

Ergänzt am 2026-08-17 auf Bitte des Betreibers.

Die bisherigen Eimer messen den Abstand zur nächsten BENANNTEN Farbe.
Sie beantworten „hat diese Farbe einen Namen?" — aber nicht die Frage,
die #00e5ff monatelang überstehen liess: **erreicht das Theme sie
überhaupt?**

``Style::themed()`` ersetzt Werte, die in der Abbildungstabelle in
``src/gui/styles/ThemeQss.cpp`` als Schlüssel stehen. Ein Literal, das
dort NICHT vorkommt, ist für jeden Palettenwechsel unsichtbar. Es
überlebt jede Umstellung, ohne dass irgendwo etwas rot wird — und genau
das ist an einer Farbe das Tückische: sie funktioniert weiter, sie
stimmt nur nicht mehr.

Die drei ΔE-Eimer sagten dazu nichts. Eine Farbe kann einen Namen
haben, dicht an der Palette liegen, und trotzdem eingefroren sein,
weil ihr Wert in der Tabelle fehlt.

Gemeldet werden deshalb zwei Gruppen, nach Vorkommen sortiert:

* **eingefroren** — Wert nicht in der Tabelle. Entweder eine Zeile
  dort ergänzen oder das Literal auf einen Wert ziehen, den die Tabelle
  kennt.
* **eingefroren UND namenlos (ΔE > 18)** — die schlimmste Sorte: keine
  Konstante, kein Theme-Zugriff, keine Ähnlichkeit zu irgendetwas
  Benanntem. Da hat jemand eine Farbe erfunden und niemand hat sie
  wieder eingesammelt.

Zwei Gruppen bleiben absichtlich draussen, weil sie sonst das Bild
zudecken:

* Dateien in ``EXEMPT`` — dieselbe Befreiung wie in der übrigen
  Inventur.
* Farben, die nur in Kommentaren stehen, kann das Skript nicht von
  Code unterscheiden; es liest zeilenweise. Das war vorher schon so
  und wird hier nicht schlimmer.

Ausgeschlossen als Ziel sind die ``kEqBand*``-Töne. Die existieren, um
acht überlagerte Entzerrerkurven auseinanderzuhalten, und ein Knopf im
Hauptfenster daran zu binden wäre eine Kopplung, die niemand gewollt
hat.
"""

from __future__ import annotations

import collections
import math
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "src"
CONSTANTS = SRC / "gui" / "StyleConstants.h"

# Dateien, die per Definition Farbwerte tragen dürfen.
EXEMPT = {
    "StyleConstants.h",   # die Palette selbst
    "EqPalette.h",        # acht Entzerrer-Töne, die sich gegenseitig
                          # unterscheidbar halten müssen, nicht der Chrome
    "ThemeQss.cpp",       # die Theme-Tabelle
}

HEX = re.compile(r"#[0-9a-fA-F]{6}\b")
NAMED = re.compile(r'constexpr auto (k\w+)\s*=\s*"(#[0-9a-fA-F]{6})"')

# ── Die Schreibweise, die zwei Runden lang unsichtbar war ────────────
#
# 2026-08-15, am laufenden Programm gefunden: die Spektrumkurve ist
#
#     setFillColor(QColor(0x00, 0xe5, 0xff));   // default cyan trace
#
# Drei Bytes, kein #00e5ff. Die Inventur hat diese Farbe nie gezählt,
# und der Theme-Eintrag "#00e5ff" lief ins Leere — er hätte auch dann
# nicht gegriffen, wenn das Skript sie gefunden hätte, weil hier gemalt
# statt gestylt wird.
#
# Beide Formen werden jetzt erfasst. Dezimal ebenso: QColor(0, 229, 255)
# ist dieselbe Farbe und kommt im Quelltext genauso vor.
QCOLOR_HEX = re.compile(
    r"QColor\s*\(\s*0x([0-9a-fA-F]{1,2})\s*,\s*0x([0-9a-fA-F]{1,2})"
    r"\s*,\s*0x([0-9a-fA-F]{1,2})\s*\)")
QCOLOR_DEC = re.compile(
    r"QColor\s*\(\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})\s*\)")


def to_lab(h: str) -> tuple[float, float, float]:
    """sRGB-Hex nach CIE-Lab, D65."""
    h = h.lstrip("#")

    def lin(u: float) -> float:
        return u / 12.92 if u <= 0.04045 else ((u + 0.055) / 1.055) ** 2.4

    r, g, b = (lin(int(h[i:i + 2], 16) / 255) for i in (0, 2, 4))
    x = (r * 0.4124 + g * 0.3576 + b * 0.1805) / 0.95047
    y = (r * 0.2126 + g * 0.7152 + b * 0.0722)
    z = (r * 0.0193 + g * 0.1192 + b * 0.9505) / 1.08883

    def f(t: float) -> float:
        return t ** (1 / 3) if t > 0.008856 else 7.787 * t + 16 / 116

    fx, fy, fz = f(x), f(y), f(z)
    return (116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz))


def delta_e(a: str, b: str) -> float:
    return math.dist(to_lab(a), to_lab(b))


def named_palette() -> dict[str, str]:
    text = CONSTANTS.read_text(encoding="utf-8")
    return {m.group(1): m.group(2).lower() for m in NAMED.finditer(text)}


# ── Der Vorgabewert hinter einem Themenschlüssel ist keine Drift ──────
#
# `eqInk("eq-bg", "#191919")` sieht wie ein namenloses Literal aus und
# ist das Gegenteil: der Ton HAT einen Namen (`eq-bg`), und der Hexwert
# dahinter ist der portierte Vorgabewert — hier BackColor aus Thetis'
# ucParametricEq.cs:443.
#
# Am 2026-08-18 hat --apply acht solche Werte in ParametricEqWidget.cpp
# angeglichen, darunter #191919 -> #18181a. tst_parametric_eq_widget_paint
# hat es gefangen, weil es den Thetis-Wert punktgenau prueft — und es hat
# recht damit: CLAUDE.md verlangt, portierte Konstanten unveraendert zu
# lassen. Eine „unsichtbare" Angleichung an die Hauspalette ist bei einem
# portierten Wert kein Stilgewinn, sondern ein Portierungsfehler.
#
# Erkennungsmerkmal ist der Schluessel VOR dem Literal in derselben
# Klammer. Ein Zitat auf derselben Zeile genuegt nicht: von den acht
# Stellen trug nur eine eines.
RE_KEYED_DEFAULT = re.compile(
    r'\w+\s*\(\s*"[A-Za-z0-9_.-]+"\s*,\s*"(#[0-9a-fA-F]{6})"')


def keyed_default_spans(line: str) -> list[tuple[int, int]]:
    """Bereiche der Literale, die als Vorgabewert hinter einem Schluessel stehen."""
    return [m.span(1) for m in RE_KEYED_DEFAULT.finditer(line)]


def literals() -> dict[str, list[tuple[str, int]]]:
    """Farbe -> [(Datei, Zeile), …], ohne die befreiten Dateien."""
    found: dict[str, list[tuple[str, int]]] = collections.defaultdict(list)
    for path in sorted(SRC.rglob("*")):
        if path.suffix not in {".cpp", ".h"} or path.name in EXEMPT:
            continue
        rel = path.relative_to(ROOT).as_posix()
        for n, line in enumerate(
                path.read_text(encoding="utf-8", errors="replace").splitlines(),
                start=1):
            # Kommentartext zaehlt NICHT. Ein Satz wie „war #00b4d8"
            # dokumentiert eine Ersetzung; ihn als Farbliteral zu zaehlen
            # heisst, dass jede aufgeraeumte Farbe im Bestand bleibt,
            # solange jemand erklaert hat, warum sie weg ist.
            #
            # Am 2026-08-18 sichtbar geworden: nach dem Abbilden von 182
            # Vorkommen auf die vier Rollen blieb die Zahl der Farben
            # stehen. Von 28 verbliebenen #00b4d8 standen 26 in
            # Kommentaren.
            cut = line.find("//")
            if cut >= 0:
                line = line[:cut]
            for m in HEX.finditer(line):
                found[m.group(0).lower()].append((rel, n))
            for m in QCOLOR_HEX.finditer(line):
                rgb = "#" + "".join(f"{int(g, 16):02x}" for g in m.groups())
                found[rgb].append((rel, n))
            for m in QCOLOR_DEC.finditer(line):
                vals = [int(g) for g in m.groups()]
                if any(v > 255 for v in vals):
                    continue            # keine Farbe, irgendein anderer Konstruktor
                found["#" + "".join(f"{v:02x}" for v in vals)].append((rel, n))
    return found


def classify(colour: str, palette: dict[str, str]):
    # kEqBand* sind keine Chrome-Farben. Sie halten acht überlagerte
    # Entzerrerkurven auseinander; ein Knopf im Hauptfenster, der daran
    # hängt, ändert seine Farbe, sobald jemand die EQ-Töne nachjustiert.
    usable = {k: v for k, v in palette.items() if not k.startswith("kEqBand")}
    name, value = min(usable.items(), key=lambda kv: delta_e(colour, kv[1]))
    return name, value, delta_e(colour, value)


def theme_legacy_keys() -> set[str]:
    """Die Farben, auf die die Theme-Tabelle horcht.

    ── Die Falle, die ich mir fast gebaut hätte ──────────────────────

    Nachdem die Palette verschoben war, meldete ``--apply`` plötzlich
    733 ersetzbare Vorkommen statt 241. Der Grund: die Literale in den
    Widgets tragen noch die ALTEN Nereus-Werte, und die liegen dicht
    neben den neuen — ``#0f0f1a`` ist von ``#08080a`` kaum zu
    unterscheiden.

    Sie zu ersetzen wäre genau falsch. ``#0f0f1a`` ist der Schlüssel,
    an dem die Theme-Tabelle die Rolle ``app-bg`` erkennt; wird er
    weggeschrieben, findet ``themed()`` nichts mehr und die Theme-Datei
    verliert den Zugriff auf die Fläche.

    Erst hätte ich hunderte Dateien „aufgeräumt", danach hätte das
    Theme an genau diesen Stellen nicht mehr gegriffen — und die
    Ursache wäre nirgends aufgeschrieben gewesen.
    """
    src = (SRC / "gui" / "styles" / "ThemeQss.cpp")
    if not src.exists():
        return set()
    return {m.lower() for m in re.findall(
        r'\{\s*"[a-z0-9-]+"\s*,\s*"(#[0-9a-fA-F]{6})"', src.read_text())}


# Dateien, in denen ALTE Hexwerte kein Vorkommen sind, sondern
# SCHLUESSEL. ThemeQss.cpp bildet „frueher stand hier #00b4d8" auf die
# heutige Marke ab; wird der linke Wert ersetzt, erkennt die Tabelle die
# alte Farbe nicht mehr und ein Palettenwechsel geht daran vorbei.
#
# Am 2026-08-18 ist das passiert — nicht durch --apply (das fragt
# theme_legacy_keys() und haette es verweigert), sondern durch ein
# Einmal-Skript, das die vier Rollen abgebildet hat und diesen Schutz
# nicht kannte. Zwei Zeilen bekamen sogar denselben Schluessel
# (sel-bg und sel-border beide #4a7ba8). tst_theme_qss und
# tst_theme_filter haben es gefangen.
#
# Der Schutz steht jetzt hier, wo ihn jedes Skript sieht, das die
# Inventur benutzt — und nicht nur in apply_collapse.
THEME_TABLE_FILES = {"ThemeQss.cpp"}


def is_theme_table(path) -> bool:
    """Datei, in der Hexwerte Schluessel sind und nicht Vorkommen."""
    return getattr(path, "name", str(path)).rsplit("/", 1)[-1] in THEME_TABLE_FILES


def apply_collapse(buckets, dry: bool = False) -> int:
    """Schritt 1: die ununterscheidbaren Farben auf ihr Ziel setzen."""
    protected = theme_legacy_keys()
    plan: dict[str, str] = {}
    skipped = 0
    for colour, _n, name, value, _d, _hits in buckets[0]:
        if colour in protected:
            skipped += 1
            continue
        plan[colour] = value
    if skipped:
        print(f"{skipped} Farbe(n) übersprungen: sie sind Schlüssel der "
              f"Theme-Tabelle.\nWerden sie ersetzt, verliert die "
              f"Theme-Datei den Zugriff darauf.\n")
    if not plan:
        print("Nichts zu tun.")
        return 0

    # Ein Ziel darf nicht selbst Quelle sein — sonst hängt das Ergebnis
    # von der Reihenfolge ab. Kann bei ΔE < 8 nicht vorkommen, weil
    # Ziele immer benannt sind und Quellen nie, aber die Annahme
    # aufzuschreiben kostet nichts und fängt eine spätere Änderung.
    clash = set(plan) & set(plan.values())
    if clash:
        print(f"Abbruch: {clash} ist Quelle und Ziel zugleich",
              file=sys.stderr)
        return 2

    pat = re.compile("|".join(re.escape(c) for c in plan) + r"\b",
                     re.IGNORECASE)
    touched, edits = 0, 0
    for path in sorted(SRC.rglob("*")):
        if path.suffix not in {".cpp", ".h"} or path.name in EXEMPT:
            continue
        if is_theme_table(path):
            continue
        text = path.read_text(encoding="utf-8")
        # Zeilenweise, damit die geschuetzten Bereiche je Zeile bekannt
        # sind. Vorher lief die Ersetzung ueber die ganze Datei und
        # konnte einen portierten Vorgabewert nicht von einem freien
        # Literal unterscheiden.
        lines = text.split("\n")
        n = 0
        for i, line in enumerate(lines):
            spans = keyed_default_spans(line)
            def repl(m):
                nonlocal n
                for a, b in spans:
                    if m.start() >= a and m.end() <= b:
                        return m.group(0)      # portierter Vorgabewert
                n += 1
                return plan[m.group(0).lower()]
            lines[i] = pat.sub(repl, line)
        out = "\n".join(lines)
        if n:
            touched += 1
            edits += n
            print(f"  {n:>3}  {path.relative_to(ROOT).as_posix()}")
            if not dry:
                path.write_text(out, encoding="utf-8")
    verb = "wären" if dry else "wurden"
    print(f"\n{edits} Vorkommen in {touched} Dateien {verb} angeglichen "
          f"({len(plan)} Farben).")
    if not dry:
        print("Prüfen mit:  git diff --stat")
    return 0


def main() -> int:
    if not CONSTANTS.exists():
        print(f"nicht gefunden: {CONSTANTS}", file=sys.stderr)
        return 2

    palette = named_palette()
    by_value = collections.defaultdict(list)
    for k, v in palette.items():
        by_value[v].append(k)

    found = literals()
    total = sum(len(v) for v in found.values())
    orphans = {c: hits for c, hits in found.items() if c not in by_value}
    orphan_hits = sum(len(v) for v in orphans.values())

    buckets: dict[int, list] = collections.defaultdict(list)
    for colour, hits in orphans.items():
        name, value, d = classify(colour, palette)
        buckets[0 if d < 8 else (1 if d < 18 else 2)].append(
            (colour, len(hits), name, value, d, hits))

    mode = sys.argv[1] if len(sys.argv) > 1 else ""

    if mode == "--collapse":
        print("Namenlose Farben, die von einer benannten nicht zu "
              "unterscheiden sind (ΔE < 8).")
        print("Ersetzen ändert das Bild nicht.\n")
        for colour, n, name, value, d, hits in sorted(
                buckets[0], key=lambda r: -r[1]):
            print(f"{colour}  {n:>3}×  →  Style::{name} ({value})  ΔE {d:.1f}")
            for rel, line in hits[:4]:
                print(f"        {rel}:{line}")
            if len(hits) > 4:
                print(f"        … und {len(hits) - 4} weitere")
        return 0

    if mode in ("--apply", "--apply-dry"):
        if not buckets[0]:
            print("Nichts zu tun — der Eimer ΔE < 8 ist leer.")
            return 0
        print("Schritt 1: ununterscheidbare Farben auf ihr Ziel setzen.\n")
        return apply_collapse(buckets, dry=(mode == "--apply-dry"))

    if mode == "--unmapped":
        mapped = theme_legacy_keys()
        if not mapped:
            print("Die Abbildungstabelle ist leer oder nicht lesbar "
                  "(src/gui/styles/ThemeQss.cpp).", file=sys.stderr)
            print("Ohne sie ist die Frage 'erreicht das Theme diese "
                  "Farbe?' nicht zu beantworten — kein Ergebnis ist "
                  "besser als ein leeres.", file=sys.stderr)
            return 2

        # Namenlos UND weit weg: die Farben aus Eimer 2.
        far = {r[0] for r in buckets[2]}
        frozen = {c: hits for c, hits in found.items() if c not in mapped}

        worst = {c: h for c, h in frozen.items() if c in far}
        rest  = {c: h for c, h in frozen.items() if c not in far}

        print("Farben, die kein Theme erreicht — der Wert steht nicht in")
        print("der Abbildungstabelle (src/gui/styles/ThemeQss.cpp).")
        print("Sie ueberleben jeden Palettenwechsel, ohne dass etwas "
              "rot wird.\n")

        def dump(title, group, limit):
            if not group:
                return
            n = sum(len(h) for h in group.values())
            print(f"── {title}: {len(group)} Farben, {n} Vorkommen ───")
            for colour, hits in sorted(group.items(),
                                       key=lambda kv: -len(kv[1])):
                name, value, d = classify(colour, palette)
                print(f"{colour}  {len(hits):>3}×   naechste benannte: "
                      f"{name} {value}  ΔE {d:.0f}")
                for rel, line in hits[:limit]:
                    print(f"        {rel}:{line}")
                if len(hits) > limit:
                    print(f"        … und {len(hits) - limit} weitere")
            print()

        dump("eingefroren UND namenlos (ΔE > 18)", worst, 4)
        dump("eingefroren", rest, 2)
        return 0

    if mode == "--orphans":
        print("Eigenständige Farben (ΔE > 18). Jede braucht einen Namen "
              "in StyleConstants.h,")
        print("oder sie fällt weg.\n")
        for colour, n, name, value, d, hits in sorted(
                buckets[2], key=lambda r: -r[1]):
            print(f"{colour}  {n:>3}×   nächste: {name} {value}  ΔE {d:.0f}")
            for rel, line in hits[:3]:
                print(f"        {rel}:{line}")
        return 0

    files = {rel for hits in found.values() for rel, _ in hits}
    print("── Farbinventur ────────────────────────────────────────────")
    print(f"  Literale gesamt        {total:>6}")
    print(f"  in Dateien             {len(files):>6}")
    print(f"  verschiedene Farben    {len(found):>6}")
    print(f"  davon benannt          {len(found) - len(orphans):>6}")
    print(f"  davon namenlos         {len(orphans):>6}"
          f"   ({orphan_hits} Vorkommen)")
    print()
    print("── Die namenlosen, nach Abstand zur nächsten benannten ─────")
    labels = [
        ("ΔE < 8    nicht unterscheidbar", "ersetzbar, keine Entscheidung"),
        ("ΔE 8–18   sichtbar, aber nah  ", "eine Entscheidung pro Farbe"),
        ("ΔE > 18   eigene Farbe        ", "braucht einen Namen"),
    ]
    for i, (head, note) in enumerate(labels):
        n = sum(r[1] for r in buckets[i])
        print(f"  {head}  {len(buckets[i]):>4} Farben  {n:>4} Vorkommen"
              f"   — {note}")
    print()
    mapped = theme_legacy_keys()
    if mapped:
        frozen = {c: h for c, h in found.items() if c not in mapped}
        frozen_hits = sum(len(h) for h in frozen.values())
        far = {r[0] for r in buckets[2]}
        worst = len([c for c in frozen if c in far])
        print("── Was kein Theme erreicht ─────────────────────────────────")
        print(f"  nicht in der Abbildungstabelle  {len(frozen):>4} Farben"
              f"  {frozen_hits:>4} Vorkommen")
        print(f"  davon auch namenlos (ΔE > 18)   {worst:>4} Farben")
        print("  — diese folgen keinem Palettenwechsel. Der Fehlertyp,")
        print("    der #00e5ff monatelang ueberstehen liess.")
        print()

    print("  python3 tools/colour_audit.py --collapse   die ersetzbaren")
    print("  python3 tools/colour_audit.py --orphans    die eigenständigen")
    print("  python3 tools/colour_audit.py --unmapped   die eingefrorenen")
    print("  python3 tools/colour_audit.py --apply-dry  Schritt 1 vorführen")
    print("  python3 tools/colour_audit.py --apply      Schritt 1 ausführen")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
