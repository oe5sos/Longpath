#!/usr/bin/env python3
"""Die Station fahren: anmelden, offen halten, Telefone hereinlassen.

    python3 handfunke/fernzugriff/fuehren.py --dienst wss://rv.example.com/

Zeigt die Stations-Id, die ins Telefon gehoert, und was gerade los ist.
Ohne `--dienst` passiert nichts -- ohne einen Vermittlungsdienst im
Internet gibt es keinen Weg von draussen, und einen zu raten waere
schlimmer als keiner.

Geraete aufnehmen und wieder loswerden:

    python3 handfunke/fernzugriff/fuehren.py --koppeln <schluessel> --name "iPhone"
    python3 handfunke/fernzugriff/fuehren.py --zeigen
    python3 handfunke/fernzugriff/fuehren.py --widerrufen <id>

Bequemer geht es mit einem Tipp: `--koppeln-offen` oeffnet beim Start
ein Fenster, und waehrend es offen ist, nimmt die Station den Schluessel
eines Telefons AUS DEM EIGENEN NETZ entgegen -- auf der Seite ein Knopf
statt 122 abgetippter Zeichen. Die Schranken stehen in `koppeln.py`.

Bis die Kopplung per Code gebaut ist (SPAKE2, fuer die es im Browser
derzeit nichts Geprueftes gibt), koppelt man also einmal zu Hause.
Danach traegt der Fernzugriff von ueberall.
"""

from __future__ import annotations

import argparse
import asyncio
import logging
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from geraete import Geraeteliste  # noqa: E402
from koppeln import Koppelstelle, antwort  # noqa: E402
from sitzung import Ziel  # noqa: E402
from station import Station  # noqa: E402

log = logging.getLogger("fernzugriff.fuehren")

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


def _starte_koppelstelle(stelle, port: int) -> None:
    """Die Koppelstelle als kleiner HTTP-Dienst, in einem eigenen Faden.

    Sie laeuft NEBEN der Station und nicht in ihr: die Station haelt eine
    Verbindung, die nicht abreissen darf, und ein Webdienst in derselben
    Schleife waere ein Weg, sie aufzuhalten.
    """
    import threading
    from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

    class Griff(BaseHTTPRequestHandler):
        def do_POST(self):  # noqa: N802
            laenge = int(self.headers.get("Content-Length") or 0)
            rumpf = self.rfile.read(min(laenge, 4096))
            kode, aus = antwort(stelle, rumpf, self.client_address[0])
            self.send_response(kode)
            self.send_header("Content-Type", "application/json")
            # Die Seite kommt von einem anderen Port.
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Content-Length", str(len(aus)))
            self.end_headers()
            self.wfile.write(aus)

        def do_OPTIONS(self):  # noqa: N802
            self.send_response(204)
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.end_headers()

        def log_message(self, *_a):
            pass   # das Protokoll fuehrt die Koppelstelle selbst

    dienst = ThreadingHTTPServer(("0.0.0.0", port), Griff)
    threading.Thread(target=dienst.serve_forever, daemon=True).start()
    log.info("Koppelstelle auf Port %d", port)


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
    p.add_argument("--koppeln-offen", action="store_true",
                   help="beim Start ein Koppelfenster oeffnen (eigenes Netz, einmalig)")
    p.add_argument("--koppel-port", type=int, default=8790,
                   help="Port der Koppelstelle (Vorgabe: %(default)s)")
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

    stelle = Koppelstelle(liste)
    if getattr(a, "koppeln_offen", False):
        stelle.oeffne()
        print(f"Koppelfenster offen: auf der Seite auf KOPPELN tippen "
              f"(noch {stelle.rest_s:.0f} s)")
    _starte_koppelstelle(stelle, a.koppel_port)

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
