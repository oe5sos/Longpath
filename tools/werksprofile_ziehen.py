#!/usr/bin/env python3
# tools/werksprofile_ziehen.py  (Longpath)
#
# Zieht die Werksvorgabe der Layoutprofile (resources/layout/werksprofile.json)
# aus einer Longpath-Einstellungsdatei. Die Einstellungsdatei wird NUR
# GELESEN, nie geschrieben.
#
#   python3 tools/werksprofile_ziehen.py                  # Standardpfad, N und F
#   python3 tools/werksprofile_ziehen.py --pruefen        # nur pruefen, nichts schreiben
#   python3 tools/werksprofile_ziehen.py --einstellungen PFAD --profile Neu FT8
#
# Warum es das gibt (Betreiber, 2026-11-18): „ich finde, dass mein standard
# profil auch als ausgangsbasis herangezogen werden soll“. Wer Longpath frisch
# installiert, startet mit diesen Profilen statt mit einem einzelnen
# „Standard“, siehe LayoutProfiles::ladeWerksvorgabe().
#
# Das Repo ist oeffentlich. Darum wird nicht ausgeschnitten, was privat ist,
# sondern NUR UEBERNOMMEN, was als harmlos bekannt ist (Erlaubnisliste): ein
# neuer Zustandsschluessel, den eine spaetere Fassung mitschreibt, faellt
# heraus, bis ihn jemand hier eintraegt. Danach prueft eine zweite, davon
# unabhaengige Suche das Ergebnis auf Rufzeichen, Adressen, Pfade, Schirm-
# namen; findet sie etwas, wird nichts geschrieben.
#
# Modification history (Longpath)
#   2026-11-18  angelegt (Martin Fischer, OE5SOS; mit KI-Hilfe: Claude Code)

import argparse
import json
import pathlib
import re
import sys
import xml.etree.ElementTree as ET

# Betreiber 2026-11-18: „nur N und F".
STANDARDPROFILE = ["Neu", "FT8"]

# Ein 1440x900-Schirm (kleinstes MacBook-Air-Mass) mit 25 Punkten Menueleiste:
# darauf wird die Vorgabe umgerechnet. Auf groesseren Schirmen zieht der
# Profilrahmen (MainWindow::schwebendeNachProfilrahmen) sie beim Start hoch.
ZIEL_RAHMEN = [0, 25, 1440, 875]
ZIEL_SCHIRM = (1440, 900)

# Zustandsschluessel je Profil, die uebernommen werden. NICHT dabei:
#   worldImage  -- ein Dateipfad mit dem Benutzernamen; fehlt er, bleibt die
#                  eingebaute Weltkarte.
ERLAUBT_ZUSTAND = {
    "visible", "order", "floatingApplets", "floatingPans", "containerGeometry",
    "kombiInhalt", "kombiInhalt2", "panReadouts", "rotor", "rotorDockVisible",
    "splitter", "mainWindow", "contestPanels", "contestLogbook",
}
# Je schwebendem Fenster. NICHT dabei: "screen" (Name des Bildschirms).
ERLAUBT_LAGE = {"dockIndex", "dx", "dy", "x", "y", "w", "h"}
ERLAUBT_HAUPTFENSTER = {"fullScreen", "maximized", "rahmen"}

# Panels, die ein fremdes Geraet voraussetzen und NICHT von selbst verborgen
# bleiben (kein setAvailable()-Schalter in MainWindow). Sie werden in der
# Vorgabe ausgeschaltet. Acom/Spe/Kpa500/Amp/Tuner/RfKit haengen dagegen an
# ihrem eigenen Schalter (Werk: aus) und duerfen stehen bleiben.
#   CatRig  -- erscheint beim Verbinden mit einem CAT-Geraet ohnehin selbst
#              (MainWindow: ensureVisible("CatRig")).
#   KiwiSdr, KiwiWaterfalls -- brauchen einen KiwiSDR.
GERAETEPANELS_AUS = {"CatRig", "KiwiSdr", "KiwiWaterfalls"}


def standardpfad() -> pathlib.Path:
    home = pathlib.Path.home()
    if sys.platform == "darwin":
        return home / "Library/Preferences/Longpath/Longpath.settings"
    return home / ".config/Longpath/Longpath.settings"


def lies_profile(pfad: pathlib.Path) -> dict:
    wurzel = ET.parse(pfad).getroot()
    el = wurzel.find("LayoutProfiles")
    if el is None or not (el.text or "").strip():
        sys.exit(f"{pfad}: kein Eintrag LayoutProfiles")
    return json.loads(el.text)


def nur(karte: dict, erlaubt: set) -> dict:
    return {k: v for k, v in karte.items() if k in erlaubt}


def umrechner(alt_rahmen):
    """Liefert eine Funktion, die eine Lage vom alten Rahmen in ZIEL_RAHMEN
    umrechnet. dx/dy zaehlen ab der Ecke der Arbeitsflaeche, x/y absolut."""
    ax, ay, aw, ah = alt_rahmen
    zx, zy, zw, zh = ZIEL_RAHMEN
    fx, fy = zw / aw, zh / ah

    def lage(l: dict) -> dict:
        n = dict(l)
        if "w" in n: n["w"] = round(n["w"] * fx)
        if "h" in n: n["h"] = round(n["h"] * fy)
        if "dx" in n: n["dx"] = round(n["dx"] * fx)
        if "dy" in n: n["dy"] = round(n["dy"] * fy)
        if "x" in n: n["x"] = round(zx + (n["x"] - ax) * fx)
        if "y" in n: n["y"] = round(zy + (n["y"] - ay) * fy)
        # Rundung darf nicht ueber den Rand tragen.
        for a, b, rand in (("x", "w", zx + zw), ("y", "h", zy + zh)):
            if a in n and b in n and n[a] + n[b] > rand:
                n[a] = max(0, rand - n[b])
        for a, b, rand in (("dx", "w", zw), ("dy", "h", zh)):
            if a in n and b in n and n[a] + n[b] > rand:
                n[a] = max(0, rand - n[b])
        return n

    return lage


def bereinige_zustand(s: dict) -> dict:
    s = nur(s, ERLAUBT_ZUSTAND)
    hw = nur(s.get("mainWindow", {}), ERLAUBT_HAUPTFENSTER)
    alt = hw.get("rahmen")
    if not (isinstance(alt, list) and len(alt) == 4 and alt[2] > 0 and alt[3] > 0):
        alt = list(ZIEL_RAHMEN)
    lage = umrechner(alt)
    hw["rahmen"] = list(ZIEL_RAHMEN)
    s["mainWindow"] = hw

    for schluessel in ("floatingApplets", "floatingPans", "containerGeometry"):
        if schluessel in s:
            s[schluessel] = {
                k: lage(nur(v, ERLAUBT_LAGE))
                for k, v in s[schluessel].items()
                if k not in GERAETEPANELS_AUS and isinstance(v, dict)
            }
    if "contestPanels" in s:
        s["contestPanels"] = {
            k: lage(nur(v, ERLAUBT_LAGE)) if isinstance(v, dict) else v
            for k, v in s["contestPanels"].items()
        }
    vis = dict(s.get("visible", {}))
    for g in GERAETEPANELS_AUS:
        if g in vis:
            vis[g] = False
    s["visible"] = vis
    for k in ("kombiInhalt", "kombiInhalt2"):
        if k in s:
            s[k] = [x for x in s[k] if x not in GERAETEPANELS_AUS]
    return s


def ziehe(roh: dict, namen: list) -> dict:
    nach_name = {p.get("name"): p for p in roh.get("profiles", [])}
    fehlt = [n for n in namen if n not in nach_name]
    if fehlt:
        sys.exit(f"Profile fehlen in der Einstellungsdatei: {', '.join(fehlt)}")
    profile = []
    for n in namen:
        p = nach_name[n]
        profile.append({
            "name": n,
            "state": bereinige_zustand(p.get("state", {})),
            "bands": sorted(int(x) for x in p.get("bands", [])),
            "modes": sorted(int(x) for x in p.get("modes", [])),
            "digis": sorted(int(x) for x in p.get("digis", [])),
        })
    # Startprofil ist IMMER Profil 1 (Betreiber 2026-11-12); "current"
    # sagt dasselbe, damit die Datei nicht zwei Antworten gibt.
    return {"current": namen[0], "profiles": profile}


# ── Zweite, unabhaengige Pruefung ──────────────────────────────────────

VERBOTEN = [
    (re.compile(r"\b\d{1,3}(?:\.\d{1,3}){3}\b"), "IPv4-Adresse"),
    (re.compile(r"\b[0-9A-Fa-f]{2}(?:[:-][0-9A-Fa-f]{2}){5}\b"), "MAC-Adresse"),
    (re.compile(r"@"), "E-Mail/@"),
    (re.compile(r"/Users/|/home/|[A-Za-z]:\\\\"), "Dateipfad"),
    (re.compile(r"(?i)display|retina|monitor|\bDELL\b|\bLG\b"), "Bildschirmname"),
    (re.compile(r"(?i)apikey|api_key|password|passwort|token|secret"), "Schluessel"),
    # Amateurfunk-Rufzeichen: Praefix mit Ziffer, danach 1-4 Buchstaben.
    (re.compile(r"\b[A-Z0-9]{1,3}\d[A-Z]{1,4}\b"), "Rufzeichen"),
]


def alle_texte(x):
    if isinstance(x, dict):
        for k, v in x.items():
            yield str(k)
            yield from alle_texte(v)
    elif isinstance(x, list):
        for v in x:
            yield from alle_texte(v)
    elif isinstance(x, str):
        yield x


def alle_lagen(x):
    if isinstance(x, dict):
        if {"w", "h"} <= x.keys() and ({"x", "y"} <= x.keys() or {"dx", "dy"} <= x.keys()):
            yield x
        for v in x.values():
            yield from alle_lagen(v)
    elif isinstance(x, list):
        for v in x:
            yield from alle_lagen(v)


def pruefe(vorgabe: dict, eigene: list) -> list:
    funde = []
    for t in alle_texte(vorgabe):
        for muster, art in VERBOTEN:
            if muster.search(t):
                funde.append(f"{art}: {t!r}")
        for e in eigene:
            if e and e.lower() in t.lower():
                funde.append(f"Wert aus der Einstellungsdatei: {t!r}")
    zx, zy, zw, zh = ZIEL_RAHMEN
    for l in alle_lagen(vorgabe):
        if "x" in l and (l["x"] < 0 or l["y"] < 0 or l["x"] + l["w"] > ZIEL_SCHIRM[0]
                         or l["y"] + l["h"] > ZIEL_SCHIRM[1]):
            funde.append(f"Lage ausserhalb {ZIEL_SCHIRM}: {l}")
        if "dx" in l and (l["dx"] < 0 or l["dy"] < 0 or zx + l["dx"] + l["w"] > ZIEL_SCHIRM[0]
                          or zy + l["dy"] + l["h"] > ZIEL_SCHIRM[1]):
            funde.append(f"Lage ausserhalb {ZIEL_SCHIRM}: {l}")
    return funde


def eigene_werte(pfad: pathlib.Path) -> list:
    """Werte aus der Einstellungsdatei, die nie in der Vorgabe stehen duerfen:
    Rufzeichen, Locator, Name, Adressen."""
    werte = []
    for el in ET.parse(pfad).getroot().iter():
        tag = el.tag.lower()
        if any(s in tag for s in ("callsign", "locator", "gridsquare", "operator",
                                  "email", "address", "mac", "host", "qth")):
            t = (el.text or "").strip()
            if 3 <= len(t) <= 64 and not t.lower() in ("true", "false"):
                werte.append(t)
    return werte


def main() -> int:
    hier = pathlib.Path(__file__).resolve().parent.parent
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--einstellungen", type=pathlib.Path, default=standardpfad())
    ap.add_argument("--profile", nargs="+", default=STANDARDPROFILE)
    ap.add_argument("--ziel", type=pathlib.Path,
                    default=hier / "resources/layout/werksprofile.json")
    ap.add_argument("--pruefen", action="store_true",
                    help="nur die vorhandene Zieldatei pruefen")
    a = ap.parse_args()

    if a.pruefen:
        vorgabe = json.loads(a.ziel.read_text(encoding="utf-8"))
        eigene = eigene_werte(a.einstellungen) if a.einstellungen.exists() else []
    else:
        vorgabe = ziehe(lies_profile(a.einstellungen), a.profile)
        eigene = eigene_werte(a.einstellungen)

    funde = pruefe(vorgabe, eigene)
    if funde:
        print("NICHT geschrieben -- gefunden:", file=sys.stderr)
        for f in funde:
            print("  " + f, file=sys.stderr)
        return 1
    if a.pruefen:
        print(f"{a.ziel}: sauber, {len(vorgabe['profiles'])} Profile")
        return 0
    a.ziel.parent.mkdir(parents=True, exist_ok=True)
    a.ziel.write_text(json.dumps(vorgabe, indent=2, sort_keys=True,
                                 ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"{a.ziel}: {', '.join(a.profile)} (Start: {a.profile[0]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
