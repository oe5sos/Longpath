#!/usr/bin/env python3
"""Die Station fahren: anmelden, offen halten, Telefone hereinlassen.

    python3 handfunke/fernzugriff/fuehren.py --dienst wss://rv.example.com/

Zeigt die Stations-Id, die ins Telefon gehoert, und was gerade los ist.
Ohne `--dienst` passiert nichts -- ohne einen Vermittlungsdienst im
Internet gibt es keinen Weg von draussen, und einen zu raten waere
schlimmer als keiner.

Geraete aufnehmen und wieder loswerden:

    python3 handfunke/fernzugriff/fuehren.py --koppeln <schluessel> --name "Martins iPhone"
    python3 handfunke/fernzugriff/fuehren.py --zeigen
    python3 handfunke/fernzugriff/fuehren.py --widerrufen <id>

Den Schluessel zeigt die Seite im Kopplungsblatt. Das ist der Weg, bis
die Kopplung per Code gebaut ist -- dann genuegt eine Nummer und zwei
Woerter, und nichts muss mehr abgetippt werden.
"""

from __future__ import annotations

import argparse
import asyncio
import logging
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from geraete import Geraeteliste  # noqa: E402
from sitzung import Ziel  # noqa: E402
from station import Station  # noqa: E402

# Neben dem Schluessel der Seite, ausserhalb des Quellbaums.
ORT = Path.home() / ".config" / "longpath-fernzugriff"


def zeige(lage) -> None:  # noqa: ANN001
    teile = [f"Id {lage.ident}"]
    if lage.nummer is not None:
        teile.append(f"Nummer {lage.nummer}")
    teile.append("angemeldet" if lage.angemeldet else "nicht angemeldet")
    if lage.sitzungen:
        teile.append(f"{lage.sitzungen} Sitzung(en)")
    if lage.letzter_fehler:
        teile.append(f"zuletzt: {lage.letzter_fehler}")
    print("  ".join(teile), flush=True)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--dienst", help="wss://… des Rendezvous")
    p.add_argument("--longpath", default="ws://127.0.0.1:50001",
                   help="Longpaths TCI-Server (Vorgabe: %(default)s)")
    p.add_argument("--token", default="", help="TCI-Token, falls Longpath eins verlangt")
    p.add_argument("--ort", default=str(ORT), help="wo Schluessel und Geraeteliste liegen")
    p.add_argument("--koppeln", metavar="SCHLUESSEL", help="ein Geraet aufnehmen")
    p.add_argument("--name", default="Telefon")
    p.add_argument("--zeigen", action="store_true", help="die Geraete auflisten")
    p.add_argument("--widerrufen", metavar="ID", help="ein Geraet entfernen")
    p.add_argument("--leise", action="store_true")
    a = p.parse_args()

    logging.basicConfig(
        level=logging.WARNING if a.leise else logging.INFO,
        format="%(asctime)s %(levelname)s %(message)s",
        datefmt="%H:%M:%S",
    )

    ort = Path(a.ort)
    liste = Geraeteliste(ort / "geraete.json")

    if a.zeigen:
        if not len(liste):
            print("kein Geraet gekoppelt")
        for g in liste:
            print(f"{g.ident}  {g.name}  ({g.art})")
        return 0

    if a.koppeln:
        g = liste.koppele(a.koppeln.strip(), a.name)
        print(f"aufgenommen: {g.ident}  {g.name}")
        return 0

    if a.widerrufen:
        print("entfernt" if liste.widerrufe(a.widerrufen) else "nicht gefunden")
        return 0

    if not a.dienst:
        p.error("ohne --dienst gibt es keinen Weg von draussen")

    st = Station(
        a.dienst,
        ort / "station.pem",
        beim_wechsel=zeige,
        geraete=liste,
        ziel=Ziel(url=a.longpath, token=a.token),
    )
    print(f"Stations-Id: {st.lage.ident}")
    print(f"{len(liste)} Geraet(e) gekoppelt")
    try:
        asyncio.run(st.halte_offen())
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
