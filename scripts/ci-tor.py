#!/usr/bin/env python3
"""Das Merge-Tor: ist die CI eines PR WIRKLICH durch?

Warum dieses Skript existiert
-----------------------------
`gh pr checks N` war das Tor ("mergen nur bei Exitcode 0").  Am
2026-10-21 hat es zweimal gelogen, und zwar in dieselbe Richtung --
gruen, obwohl nichts durch war:

* **Abgebrochen zaehlt nicht als Fehlschlag.**  Bei #291 und #297
  druckte `gh pr checks` die Zeile
  `Build (macOS Apple Silicon (2/2))  fail` und endete trotzdem mit
  **Exitcode 0**.  Der Grund war nicht Rot, sondern Abgebrochen: beide
  Laeufe hatten `conclusion: cancelled`.  Die Anmerkung der Aufgabe
  sagte es wortwoertlich -- "The run was canceled by @oe5sos." -- und
  drei Sekunden spaeter starteten die macOS-Aufgaben des main-Laufs.
  Es hat also jemand die Warteschlange freigeraeumt, und beide PRs
  sahen danach fertig aus, obwohl ihr Testbau auf macOS nie lief.

* **Zwei Arbeitsfolgen unter derselben SHA.**  Fuer jeden PR laufen
  `CI` und `CodeQL` unter derselben Kopf-SHA und demselben
  Zeitstempel.  `CodeQL` hat genau eine Aufgabe (`Analyze (cpp)`) und
  ist in zwei Minuten durch.  Wer den falschen Lauf ansieht, liest
  `success` und glaubt, alles sei fertig.  Genau diese Verwechslung
  hat die erste Durchsicht in die Irre gefuehrt.

Dieses Skript nimmt deshalb NICHT die Pruefliste, sondern den Lauf:
die Arbeitsfolge `CI` fuer die Kopf-SHA des PR, und jede ihrer
Aufgaben einzeln.  Gefordert wird `success` -- `cancelled`, `failure`,
`timed_out`, `action_required` und alles noch Offene sind kein Tor.
`skipped` gilt als durch (Plattformtore in ci.yml ueberspringen
Aufgaben planmaessig).

Dass die macOS-Aufgaben lange brauchen, ist dabei NORMAL und kein
Grund zu warten oder abzubrechen: GitHub haengt an diese Laeufe selbst
die Anmerkung "Due to capacity constraints, jobs targeting macOS arm64
runners may experience longer queue times."  Am 2026-10-21 gemessen:
die zwei macOS-Scherben liefen streng NACHEINANDER, je 31 bis 42
Minuten -- also gut 70 Minuten macOS-Zeit je PR.

Aufruf
------
    python3 scripts/ci-tor.py 300
    python3 scripts/ci-tor.py 300 --json

Exitcodes
---------
    0  durch -- jede Aufgabe des CI-Laufs ist `success` oder `skipped`
    1  nicht durch (gefallen, abgebrochen oder noch offen)
    2  nichts zu lesen (kein Lauf, kein PR, `gh` nicht erreichbar)

Der Unterschied zwischen 1 und 2 ist Absicht: 2 heisst "ich weiss es
nicht", und das darf nie als "grün" durchgehen.  Ein PR mit null
Pruefungen gibt es nicht; so etwas ist immer ein Netz- oder
Rechtefehler.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys

LAGER_VORGABE = "oe5sos/Longpath"
ARBEITSFOLGE = "CI"

# `conclusion`-Werte, die eine Aufgabe als durch gelten lassen.
# `skipped` gehoert dazu, weil ci.yml Plattformtore benutzt; alles
# andere -- auch `cancelled` und `neutral` -- nicht.
DURCH = {"success", "skipped"}


def gh(*args: str) -> str:
    """`gh` aufrufen und die Ausgabe liefern. Leer bei jedem Fehler.

    Ohne Pipe und ohne Filter: eine Pipe hinter `gh` wirft den
    Exitcode weg, und genau daran ist dieses Tor schon einmal
    gescheitert (memory feedback-merge-nur-bei-gh-exit-0, zweite Form).
    """
    try:
        fertig = subprocess.run(
            ["gh", *args], capture_output=True, text=True, timeout=120
        )
    except (OSError, subprocess.TimeoutExpired) as fehler:
        print(f"[ci-tor] `gh` nicht erreichbar: {fehler}", file=sys.stderr)
        return ""
    if fertig.returncode != 0:
        print(f"[ci-tor] `gh {' '.join(args)}` endete mit "
              f"{fertig.returncode}: {fertig.stderr.strip()}", file=sys.stderr)
        return ""
    return fertig.stdout


def kopf_sha(lager: str, nummer: int) -> str:
    rohdaten = gh("pr", "view", str(nummer), "-R", lager,
                  "--json", "headRefOid,headRefName,state")
    if not rohdaten:
        return ""
    try:
        daten = json.loads(rohdaten)
    except json.JSONDecodeError:
        return ""
    return str(daten.get("headRefOid") or "")


def ci_lauf(lager: str, sha: str) -> dict | None:
    """Den Lauf der Arbeitsfolge `CI` fuer diese SHA finden.

    Nicht den neuesten Lauf des Zweigs: ein Zweig kann mehrere SHAs
    haben, und der Lauf der vorigen zaehlt nicht.  Und nicht den
    erstbesten Lauf der SHA: das waere mit gleicher
    Wahrscheinlichkeit CodeQL.
    """
    # KEIN --paginate: bei einem Endpunkt, der ein Objekt liefert,
    # haengt `gh` die Seiten ohne Trenner aneinander, und das Ergebnis
    # ist kein gueltiges JSON mehr.  Eine Kopf-SHA hat eine Handvoll
    # Laeufe; eine Seite mit 100 reicht dafuer immer.
    rohdaten = gh("api",
                  f"repos/{lager}/actions/runs?head_sha={sha}&per_page=100")
    if not rohdaten:
        return None
    try:
        treffer = json.loads(rohdaten).get("workflow_runs", [])
    except json.JSONDecodeError:
        return None
    laeufe = [l for l in treffer if l.get("name") == ARBEITSFOLGE]
    if not laeufe:
        return None
    # Der hoechste Versuch gilt: `gh run rerun --failed` legt einen
    # neuen an, und der alte bleibt mit seinem Abbruch stehen.
    laeufe.sort(key=lambda l: (l.get("run_attempt") or 0, l.get("id") or 0))
    return laeufe[-1]


def aufgaben(lager: str, lauf_id: int, versuch: int) -> list[dict]:
    rohdaten = gh("api",
                  f"repos/{lager}/actions/runs/{lauf_id}/attempts/{versuch}"
                  f"/jobs?per_page=100")
    if not rohdaten:
        return []
    try:
        return json.loads(rohdaten).get("jobs", [])
    except json.JSONDecodeError:
        return []


def wer_hat_abgebrochen(lager: str, aufgabe_id: int) -> str:
    """Die Abbruchanmerkung der Aufgabe, wenn es eine gibt.

    GitHub schreibt sie als Anmerkung an den Pruefeintrag -- "The run
    was canceled by @<name>." -- und das ist die EINZIGE Stelle, an der
    steht, wer abgebrochen hat.  Die Lauf- und Aufgabendaten selbst
    fuehren kein solches Feld.
    """
    rohdaten = gh("api", f"repos/{lager}/check-runs/{aufgabe_id}/annotations")
    if not rohdaten:
        return ""
    try:
        anmerkungen = json.loads(rohdaten)
    except json.JSONDecodeError:
        return ""
    for anmerkung in anmerkungen:
        text = str(anmerkung.get("message") or "")
        if text.startswith("The run was canceled"):
            return text
    return ""


def main() -> int:
    zerleger = argparse.ArgumentParser(
        description="Ist die CI eines PR wirklich durch?")
    zerleger.add_argument("nummer", type=int, help="die PR-Nummer")
    zerleger.add_argument("-R", "--repo", default=LAGER_VORGABE,
                          help=f"Lager (Vorgabe {LAGER_VORGABE})")
    zerleger.add_argument("--json", action="store_true",
                          help="Ergebnis als JSON statt als Text")
    # Fuer die Gegenprobe und fuer die Frage „war der Nachlauf
    # noetig?": ein bestimmter Versuch statt des hoechsten.
    zerleger.add_argument("--attempt", type=int, default=None,
                          help="einen bestimmten Versuch beurteilen "
                               "(Vorgabe: den hoechsten)")
    args = zerleger.parse_args()

    sha = kopf_sha(args.repo, args.nummer)
    if not sha:
        print(f"[ci-tor] #{args.nummer}: Kopf-SHA nicht lesbar — "
              f"nichts zu beurteilen", file=sys.stderr)
        return 2

    lauf = ci_lauf(args.repo, sha)
    if lauf is None:
        print(f"[ci-tor] #{args.nummer} @ {sha[:9]}: kein Lauf der "
              f"Arbeitsfolge {ARBEITSFOLGE!r} — nichts zu beurteilen",
              file=sys.stderr)
        return 2

    lauf_id = int(lauf["id"])
    versuch = args.attempt or int(lauf.get("run_attempt") or 1)
    liste = aufgaben(args.repo, lauf_id, versuch)
    if not liste:
        print(f"[ci-tor] Lauf {lauf_id} (Versuch {versuch}): keine "
              f"Aufgaben lesbar — nichts zu beurteilen", file=sys.stderr)
        return 2

    schlecht: list[dict] = []
    for aufgabe in liste:
        zustand = str(aufgabe.get("status") or "")
        ergebnis = str(aufgabe.get("conclusion") or "")
        if zustand != "completed" or ergebnis not in DURCH:
            eintrag = {
                "name": aufgabe.get("name"),
                "status": zustand,
                "conclusion": ergebnis or None,
            }
            if ergebnis == "cancelled":
                grund = wer_hat_abgebrochen(args.repo, int(aufgabe["id"]))
                if grund:
                    eintrag["abbruch"] = grund
            schlecht.append(eintrag)

    # Der `conclusion` des Laufs beschreibt immer den HOECHSTEN
    # Versuch. Wer einen alten beurteilt, bekommt ihn also nicht dazu;
    # dann entscheiden allein die Aufgaben dieses Versuchs.
    eigener_versuch = args.attempt is None
    durch = not schlecht and (
        not eigener_versuch or str(lauf.get("conclusion") or "") == "success")

    if args.json:
        print(json.dumps({
            "pr": args.nummer,
            "sha": sha,
            "lauf": lauf_id,
            "versuch": versuch,
            "lauf_conclusion": lauf.get("conclusion"),
            "aufgaben": len(liste),
            "durch": durch,
            "offen_oder_schlecht": schlecht,
        }, ensure_ascii=False, indent=2))
        return 0 if durch else 1

    kopf = (f"#{args.nummer} @ {sha[:9]} — Lauf {lauf_id}, Versuch "
            f"{versuch}, {len(liste)} Aufgaben")
    if durch:
        print(f"{kopf}\n[ci-tor] DURCH — jede Aufgabe success oder skipped")
        return 0

    print(f"{kopf}\n[ci-tor] NICHT DURCH ({len(schlecht)} Aufgaben):")
    for eintrag in schlecht:
        marke = eintrag["conclusion"] or eintrag["status"]
        print(f"    {marke:<16} {eintrag['name']}")
        if "abbruch" in eintrag:
            print(f"    {'':<16} └─ {eintrag['abbruch']}")
    if eigener_versuch and str(lauf.get("conclusion") or "") not in ("", "success"):
        print(f"    Lauf selbst: {lauf.get('conclusion')}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
