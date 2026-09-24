#!/usr/bin/env python3
"""Fail if any QColorDialog::getColor() call omits DontUseNativeDialog.

Betreiber am 2026-09-23: "kann longpath nicht schliessen" -- und dann,
voellig zu Recht: "trotzdem muesste der mit command Q geschlossen
werden".

Ursache: auf macOS nimmt Qt fuer QColorDialog::getColor() ohne dieses
Flag standardmaessig das NATIVE NSColorPanel. Bei ihm stand es als
schwarzes Fenster OHNE ein einziges bedienbares Element da (der
Bedienungshilfen-Baum war leer), waehrend getColor() seine eigene
Ereignisschleife fuhr. Das Programm nahm nichts mehr an: kein Cmd+Q,
kein Schliessen, nicht einmal SIGTERM.

Am 2026-09-23 abends ist das an einer Stelle (ColorSwatchButton) und am
2026-09-24 an den restlichen 64 Aufrufstellen im Baum behoben worden.
Dieser Test haelt es dort: kein neuer Aufruf darf das Flag mehr
vergessen, sonst kommt genau derselbe Fehler an einer neuen Stelle
zurueck.

Ein direkter Laufzeit-Pruefstand ist nicht moeglich: getColor() ist ein
blockierender statischer Aufruf, den kein Test ausloesen kann, ohne
selbst haengen zu bleiben -- daher der Quelltext-Scan, im selben Muster
wie die anderen scripts/verify-*.py-Pruefungen (ganzer Baum, kein
Adressraum eines Debuggers noetig).
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "src"

CALL_START = re.compile(r"QColorDialog::getColor\s*\(")

# Aufrufe, die das Flag nicht woertlich im Aufruf tragen, weil sie es
# ueber eine benannte Hilfsfunktion beziehen. Jeder Eintrag ist an der
# Definitionsstelle selbst geprueft (siehe die Datei), nicht nur hier
# behauptet.
INDIRECT_ALLOWLIST = {
    # ColorSwatchButton::pickerOptions() gibt
    # ShowAlphaChannel | DontUseNativeDialog zurueck -- siehe die
    # Definition direkt darueber in derselben Datei.
    "src/gui/ColorSwatchButton.cpp": {"pickerOptions("},
}


def find_call_span(text: str, start_paren: int) -> int:
    """Return the index just after the matching close paren."""
    depth = 0
    in_str = False
    str_ch = ""
    i = start_paren
    while i < len(text):
        c = text[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            if c == str_ch:
                in_str = False
        else:
            if c in "\"'":
                in_str = True
                str_ch = c
            elif c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
                if depth == 0:
                    return i + 1
        i += 1
    return len(text)


def main() -> int:
    failures = []
    for path in sorted(SRC.rglob("*.cpp")):
        rel = path.relative_to(ROOT).as_posix()
        text = path.read_text(encoding="utf-8")
        for m in CALL_START.finditer(text):
            open_paren = m.end() - 1
            end = find_call_span(text, open_paren)
            call_text = text[m.start():end]
            allowed_indirections = INDIRECT_ALLOWLIST.get(rel, set())
            if "DontUseNativeDialog" in call_text:
                continue
            if any(token in call_text for token in allowed_indirections):
                continue
            line = text[:m.start()].count("\n") + 1
            failures.append(f"{rel}:{line}: {call_text.splitlines()[0].strip()}")

    if failures:
        print("[no-native-color-dialog] QColorDialog::getColor() call(s) "
              "without DontUseNativeDialog:")
        for f in failures:
            print(f"  {f}")
        print()
        print("Auf macOS haengt das native NSColorPanel das Programm auf --")
        print("siehe den Kommentar im Kopf dieses Skripts. Fuege")
        print("QColorDialog::DontUseNativeDialog zu den Optionen hinzu (bei")
        print("nur zwei Argumenten braucht es davor noch einen leeren")
        print("QString()-Titel, sonst landet die Option im title-Parameter).")
        return 1

    print("[no-native-color-dialog] OK: alle QColorDialog::getColor()-"
          "Aufrufe verlangen DontUseNativeDialog.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
