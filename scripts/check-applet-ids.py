#!/usr/bin/env python3
"""Applet-Kennungen: eine je Applet, und jede muss aufloesen.

WARUM ES DIESE PRUEFUNG GIBT
============================

Am 2026-08-18 verlor „Als Fenster abloesen" das RX-Panel: es
verschwand aus der Spalte, kein Fenster erschien, und der Zustand wurde
so gespeichert. Ursache war keine kaputte Zeile, sondern NAMENSDRIFT.
Jedes Applet trug zwei Kennungen:

    Panelkennung  "Rx"   m_appletsById, AppletVisibilityController,
                         der Auswaehler, das Profilfeld "visible"
    Eigenkennung  "rx"   AppletWidget::appletId(), die Profilfelder
                         "order" und "floatingApplets", AppletStackOrder

Sie stimmten bei vier von vierzehn Applets ueberein. Wo nicht, gab jede
Nachschlagung ueber die Grenze hinweg still nullptr zurueck — kein
Absturz, keine Warnung, nur ein Nichts, das wie ein Ergebnis aussieht.

Der Test tst_applet_detach nagelt den FEHLER fest. Diese Pruefung
nagelt die URSACHE fest: sie schlaegt fehl, sobald irgendwo eine zweite
Schreibweise entsteht — auch an einer Stelle, an die heute niemand
denkt. Dieselbe Lehre wie bei den zwei Quellenlisten fuer Max Bin:
solange zwei Listen dasselbe sagen duerfen, laufen sie irgendwann
auseinander, und niemand merkt es.

WAS GEPRUEFT WIRD
=================

1. Jedes Applet, das in die Spalte gehaengt wird (panel->addApplet),
   steht in m_appletsById.
2. Jede Kennung in m_appletsById ist im Sichtbarkeitsregler angemeldet,
   und umgekehrt (ausser den Chrome-Kennungen, die kein Applet haben).
3. Fuer jedes so gefuehrte Applet ist appletId() == Panelkennung.
   EINE Schreibweise, nicht zwei.

Nicht angemeldete Applets (FM, DVK, CWX, …) stehen nicht in der Spalte
und sind darum nicht betroffen; sie werden gemeldet, aber nicht als
Fehler gezaehlt, damit ein spaeterer Einbau nicht unbemerkt bleibt.

Exit 0 = sauber, 1 = Drift.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAINWINDOW = ROOT / "src/gui/MainWindow.cpp"
APPLET_DIR = ROOT / "src/gui/applets"

# Kennungen im Auswaehler, hinter denen KEIN Applet steht: die
# Knopfleiste und die Statuszeile gehen ihren eigenen Weg
# (MainWindow::applyChromeVisibility).
CHROME_IDS = {"ChromeSpectrumButtons", "ChromeStatusBar"}

# Applets, deren Kennung von aussen hereingereicht wird statt in einer
# appletId()-Ueberschreibung zu stehen. Sie koennen mehrfach mit
# verschiedenen Kennungen auftreten — genau dafuer ist der Bauplan
# gemacht — und haben darum keine feste Zeile zum Vergleichen.
ID_FROM_CALLER = {"InstrumentApplet"}


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


def parse_mainwindow():
    """m_appletsById, registerApplet und addApplet aus MainWindow.cpp."""
    src = read(MAINWINDOW)

    # m_appletsById[QStringLiteral("Rx")] = m_rxApplet;
    by_id = dict(re.findall(
        r'm_appletsById\[\s*QStringLiteral\("([^"]+)"\)\s*\]\s*=\s*(\w+)\s*;',
        src))

    registered = set(re.findall(
        r'registerApplet\(\s*QStringLiteral\("([^"]+)"\)', src))
    # Die Chrome-Eintraege melden sich ueber eine Konstante an. Ihren
    # WERT nachschlagen statt den Konstantennamen zu nehmen — sonst
    # meldete die Pruefung eine Drift, die es nicht gibt.
    header = read(ROOT / "src/gui/MainWindow.h")
    consts = dict(re.findall(
        r'static\s+constexpr\s+auto\s+(\w+)\s*=\s*"([^"]+)"\s*;', header))
    for name in re.findall(
            r'registerApplet\(\s*QString::fromLatin1\((\w+)\)', src):
        if name in consts:
            registered.add(consts[name])
        else:
            registered.add(name)

    added = set(re.findall(r'panel->addApplet\(\s*(\w+)\s*\)', src))
    # Auskommentierte Zeilen zaehlen nicht.
    added = {m for m in added
             if not re.search(r'^\s*//.*addApplet\(\s*%s\s*\)' % re.escape(m),
                              src, re.M)}
    return by_id, registered, added


def parse_applet_ids():
    """Klassenname -> appletId()-Rueckgabe, aus den Applet-Kopfdateien."""
    out = {}
    for header in sorted(APPLET_DIR.glob("*.h")):
        src = read(header)
        m = re.search(
            r'QString\s+appletId\(\)\s*const\s*override\s*\{\s*'
            r'return\s+QStringLiteral\("([^"]+)"\)\s*;\s*\}', src)
        cls = header.stem
        if m:
            out[cls] = m.group(1)
    return out


def member_to_class(src, member):
    """m_rxApplet -> RxApplet, ueber die Stelle, an der es gebaut wird."""
    m = re.search(r'\b%s\s*=\s*new\s+(\w+)' % re.escape(member), src)
    if m:
        return m.group(1)
    # Ueber eine Zwischenvariable: `auto* txApplet = new TxApplet(...);
    # m_txApplet = txApplet;` — dem Namen einmal folgen.
    m = re.search(r'\b%s\s*=\s*(\w+)\s*;' % re.escape(member), src)
    if m:
        return member_to_class(src, m.group(1))
    return None


def same_object(src, a, b):
    """Sind zwei Namen dasselbe Objekt? (m_txApplet == txApplet)"""
    if a == b:
        return True
    return bool(re.search(r'\b%s\s*=\s*%s\s*;' % (re.escape(a), re.escape(b)),
                          src)
                or re.search(r'\b%s\s*=\s*%s\s*;' % (re.escape(b), re.escape(a)),
                             src))


def main():
    if not MAINWINDOW.exists():
        print("check-applet-ids: MainWindow.cpp nicht gefunden", file=sys.stderr)
        return 1

    src = read(MAINWINDOW)
    by_id, registered, added = parse_mainwindow()
    applet_ids = parse_applet_ids()

    problems = []
    notes = []

    if not by_id:
        problems.append("m_appletsById ist leer — hat sich der Aufbau "
                        "geaendert? Dann prueft diese Datei nichts mehr.")

    members = {member: pid for pid, member in by_id.items()}

    # ── 1. Was in der Spalte haengt, muss gefuehrt sein ──────────────
    for member in sorted(added):
        if not any(same_object(src, member, m) for m in members):
            cls = member_to_class(src, member) or "?"
            problems.append(
                f"{member} ({cls}) wird in die Spalte gehaengt, steht aber "
                f"nicht in m_appletsById — es laesst sich weder ein- noch "
                f"ausblenden, und seine Stelle in der Reihenfolge ueberlebt "
                f"keinen Neustart.")

    # ── 2. Karte und Auswaehler decken sich ─────────────────────────
    for pid in sorted(by_id):
        if pid not in registered:
            problems.append(
                f'"{pid}" steht in m_appletsById, ist aber nicht im '
                f"Auswaehler angemeldet — es taucht dort nicht auf und "
                f"laesst sich nicht zurueckholen.")
    for rid in sorted(registered - CHROME_IDS):
        if rid not in by_id:
            problems.append(
                f'"{rid}" ist angemeldet, loest aber auf kein Applet auf — '
                f"der Eintrag steht im Auswaehler und bewirkt nichts.")

    # ── 3. EINE Schreibweise je Applet ──────────────────────────────
    for pid, member in sorted(by_id.items()):
        cls = member_to_class(src, member)
        if cls is None:
            notes.append(f'"{pid}": Klasse zu {member} nicht gefunden')
            continue
        if cls in ID_FROM_CALLER:
            continue
        own = applet_ids.get(cls)
        if own is None:
            notes.append(f'"{pid}" ({cls}): keine appletId()-Zeile gefunden')
            continue
        if own != pid:
            problems.append(
                f'{cls} traegt ZWEI Schreibweisen: Panelkennung "{pid}", '
                f'appletId() "{own}". Genau daran ging am 2026-08-18 das '
                f'Abloesen des RX-Panels kaputt. Setze appletId() auf '
                f'"{pid}"; gespeicherte Aufnahmen mit der alten '
                f'Schreibweise liest AppletKeys::appletFor weiterhin.')

    # ── Applets ausserhalb der Spalte: melden, nicht bemaengeln ─────
    outside = sorted(set(applet_ids) - {member_to_class(src, m)
                                        for m in by_id.values()}
                     - ID_FROM_CALLER)
    if outside:
        notes.append("nicht in der Spalte (darum nicht geprueft): "
                     + ", ".join(outside))

    for n in notes:
        print(f"  hinweis: {n}")

    if problems:
        print()
        for p in problems:
            print(f"FAIL {p}")
        print(f"\n{len(problems)} Befund(e) — Namensdrift bei "
              f"Applet-Kennungen. Siehe src/gui/applets/AppletKeys.h.")
        return 1

    print(f"OK: {len(by_id)} Applet-Kennungen, je eine Schreibweise, "
          f"alle im Auswaehler angemeldet und aufloesbar.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
