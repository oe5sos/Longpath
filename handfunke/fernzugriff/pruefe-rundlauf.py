#!/usr/bin/env python3
"""Der ganze Weg von draussen, ohne Browser und ohne Funkgeraet.

Startet einen Rendezvous-Dienst, die TCI-Attrappe als Longpath-Ersatz und
eine Station, und laesst ein Telefon von aussen hereinkommen. Geprueft
wird, was wirklich traegt:

* ein gekoppeltes Geraet kommt durch und bekommt TCI zu hoeren,
* ein **nicht** gekoppeltes bekommt keine Antwort -- und zwar Schweigen,
  nicht eine Absage,
* ein gekoppeltes mit falscher Unterschrift auf dem Kanal kommt **nicht**
  durch (die zweite Tuer),
* vor der Anmeldung geht kein einziges TCI-Byte durch.

    python3 handfunke/fernzugriff/pruefe-rundlauf.py
"""

from __future__ import annotations

import asyncio
import contextlib
import json
import logging
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from geraet import Geraeteseite  # noqa: E402
from geraete import Geraeteliste  # noqa: E402
from sitzung import Ziel  # noqa: E402
from station import Station  # noqa: E402

WURZEL = Path(__file__).resolve().parents[2]
DIENST = WURZEL / "rendezvous" / "server"
ATTRAPPE = WURZEL / "handfunke" / "attrappe.py"
schlecht = 0


def fehler(zeile: str) -> None:
    global schlecht
    schlecht += 1
    print("  FEHLER:", zeile)


def gut(zeile: str) -> None:
    print("  ok:", zeile)


def freier_port() -> int:
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def warte_auf_port(port: int, p: subprocess.Popen, was: str) -> None:
    for _ in range(200):
        with socket.socket() as s:
            if s.connect_ex(("127.0.0.1", port)) == 0:
                return
        if p.poll() is not None:
            raus = p.stdout.read().decode() if p.stdout else ""
            raise RuntimeError(f"{was} ist gestorben:\n{raus}")
        time.sleep(0.05)
    raise RuntimeError(f"{was} nimmt nicht an")


@contextlib.contextmanager
def laeuft(befehl: list[str], cwd: Path, port: int, was: str):
    p = subprocess.Popen(befehl, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        warte_auf_port(port, p, was)
        yield p
    finally:
        p.terminate()
        with contextlib.suppress(subprocess.TimeoutExpired):
            p.wait(timeout=5)


async def warte_auf(bedingung, frist=15.0, was="etwas") -> bool:
    ende = asyncio.get_running_loop().time() + frist
    while asyncio.get_running_loop().time() < ende:
        if bedingung():
            return True
        await asyncio.sleep(0.02)
    return False


async def lauf() -> None:
    rv_port, tci_port = freier_port(), freier_port()
    url = f"ws://127.0.0.1:{rv_port}/"

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        with tempfile.NamedTemporaryFile("w", suffix=".conf", delete=False) as f:
            f.write(f"[rendezvous]\nlisten = 127.0.0.1:{rv_port}\nstun_urls =\nlog_level = warning\n")
            conf = f.name

        with laeuft([sys.executable, "-m", "nereus_rendezvous", "--config", conf],
                    DIENST, rv_port, "der Dienst"), \
             laeuft([sys.executable, str(ATTRAPPE), "--port", str(tci_port)],
                    WURZEL, tci_port, "die Attrappe"):

            liste = Geraeteliste(tmp / "geraete.json")
            st = Station(
                url,
                tmp / "station.pem",
                geraete=liste,
                ziel=Ziel(url=f"ws://127.0.0.1:{tci_port}"),
            )
            halten = asyncio.create_task(st.halte_offen())
            try:
                if not await warte_auf(lambda: st.lage.angemeldet, was="die Anmeldung"):
                    fehler("die Station hat sich nicht angemeldet")
                    return

                # ── 1. Ein Fremder bekommt Schweigen ──────────────────
                fremd = Geraeteseite(url, tmp / "fremd.pem", st.lage.ident)
                t0 = asyncio.get_running_loop().time()
                try:
                    await fremd.verbinde(frist=4.0)
                    fehler("ein nicht gekoppeltes Geraet ist hereingekommen")
                except asyncio.TimeoutError:
                    gut("ein Fremder bekommt keine Antwort (Schweigen, keine Absage)")
                except RuntimeError as e:
                    fehler(f"ein Fremder bekam eine Absage statt Schweigen: {e}")
                finally:
                    await fremd.schliesse()
                if asyncio.get_running_loop().time() - t0 < 3.5:
                    fehler("die Absage kam zu schnell -- das verraet, dass das Geraet fremd ist")

                # ── 2. Koppeln und hereinkommen ───────────────────────
                telefon = Geraeteseite(url, tmp / "telefon.pem", st.lage.ident)
                liste.koppele(telefon.schluessel_text, "Pruefstand", "phone")

                empfangen: list = []
                telefon.beim_empfang = empfangen.append
                await telefon.verbinde(frist=25.0)
                gut("das gekoppelte Geraet ist drin")

                if not await warte_auf(lambda: len(empfangen) > 0, frist=10.0,
                                       was="die ersten TCI-Nachrichten"):
                    fehler("durch den Kanal kam nichts von der Attrappe")
                else:
                    texte = [m for m in empfangen if isinstance(m, str)]
                    binaer = [m for m in empfangen if isinstance(m, (bytes, bytearray))]
                    gut(f"{len(texte)} Textnachrichten, {len(binaer)} Binaerrahmen")
                    if not any("vfo:" in m or "start" in m or "device:" in m for m in texte):
                        fehler(f"der Init-Burst fehlt; erste Nachricht: {texte[:1]}")

                # Und in die Gegenrichtung: eine Abfrage, eine Antwort.
                vorher = len(empfangen)
                telefon.sende("vfo:0,0;")
                if not await warte_auf(lambda: len(empfangen) > vorher, frist=5.0,
                                       was="die Antwort auf eine Abfrage"):
                    fehler("auf eine Abfrage kam nichts zurueck")
                else:
                    gut("eine Abfrage kommt bei der Attrappe an und wird beantwortet")

                # ── 2b. Auch Binaeres kommt durch ─────────────────────
                # Der Ton ist der eigentliche Brocken (gemessen 24 kB/s),
                # und er faehrt binaer. Ein Kanal, der nur Text traegt,
                # saehe bis hierher gesund aus.
                vorher = len(empfangen)
                telefon.sende("start;")
                telefon.sende("audio_start:0;")
                if not await warte_auf(
                    lambda: any(
                        isinstance(m, (bytes, bytearray)) for m in empfangen[vorher:]
                    ),
                    frist=10.0,
                    was="Binaerrahmen",
                ):
                    fehler("durch den Kanal kam kein einziger Binaerrahmen")
                else:
                    roh = [m for m in empfangen[vorher:] if isinstance(m, (bytes, bytearray))]
                    gut(f"{len(roh)} Binaerrahmen, groesster {max(len(m) for m in roh)} Byte")

                await telefon.schliesse()

                # ── 3. Die zweite Tuer: falsche Unterschrift ──────────
                # Dasselbe gekoppelte Geraet, aber es unterschreibt die
                # Aufforderung mit einem anderen Schluessel. Das ist der
                # Fall, der frueher durchrutschte: Einfuehrung gut,
                # Sitzung ungeprueft.
                falsch = Geraeteseite(url, tmp / "telefon.pem", st.lage.ident)
                anderer = Geraeteseite(url, tmp / "anderer.pem", st.lage.ident)
                # NUR die Anmeldung auf dem Kanal wird falsch
                # unterschrieben. Die Einfuehrung bleibt gueltig -- sonst
                # scheitert das Geraet schon an der ersten Tuer und die
                # zweite bleibt ungeprueft. (Beim ersten Anlauf war genau
                # das der Fall: der Lauf war gruen und belegte nichts.)
                falsch.anmeldeschluessel = anderer.privat
                durch = []
                falsch.beim_empfang = durch.append
                try:
                    await falsch.verbinde(frist=20.0)
                    fehler("eine falsche Unterschrift auf dem Kanal kam durch")
                except (asyncio.TimeoutError, RuntimeError):
                    gut("eine falsche Unterschrift auf dem Kanal kommt nicht durch")
                finally:
                    await falsch.schliesse()
                if durch:
                    fehler(f"vor der Anmeldung flossen {len(durch)} Nachrichten")
                else:
                    gut("vor der Anmeldung floss kein TCI")

            finally:
                halten.cancel()
                with contextlib.suppress(asyncio.CancelledError):
                    await halten
                Path(conf).unlink(missing_ok=True)


def main() -> int:
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(name)s %(message)s")
    asyncio.run(lauf())
    print()
    print("ALLES GRUEN" if schlecht == 0 else f"{schlecht} Abweichungen")
    return 0 if schlecht == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
