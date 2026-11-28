#!/usr/bin/env python3
"""Der WebSocket-Relay (Abschnitt 12), gegen den echten Dienst.

Startet Rendezvous-Dienst UND Relay aus `rendezvous/server/` (der Relay
auf seinem Unix-Socket, wie hinter Caddy), die TCI-Attrappe und eine
Station, und laesst ein Telefon herein -- **in einem nachgestellten Netz,
das nur Web durchlaesst**: aus beiden SDPs sind alle Kandidaten entfernt,
getrickelte werden verworfen. Direkt und ueber UDP geht also nichts; wenn
etwas durchkommt, dann nur durch den Relay.

Geprueft wird:

1. mit Relay kommt ein gekoppeltes Geraet durch, TCI fliesst in beide
   Richtungen, Binaerrahmen auch -- und ICE hat WIRKLICH das Paar ueber
   die Relay-Steckdose gewaehlt, die Relay-Zaehler stehen nicht auf null;
2. **Gegenprobe**: dasselbe Netz ohne Relay-Angebot -- das Geraet kommt
   NICHT durch (sonst belegte Punkt 1 nichts);
3. ein Token, den der Relay nicht kennt (`badToken`), wird nicht erneut
   versucht, und die Sitzung kommt nicht zustande;
4. im Normalbetrieb (direkter Weg vorhanden) wird der Relay NICHT
   gewaehlt und sein Bein wird nicht liegengelassen.

    python3 handfunke/fernzugriff/pruefe-relay.py
"""

from __future__ import annotations

import asyncio
import contextlib
import logging
import os
import secrets
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from geraet import Geraeteseite  # noqa: E402
from geraete import Geraeteliste  # noqa: E402
from relais import Relaisbein  # noqa: E402
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


def warte_bis(bedingung, p: subprocess.Popen, was: str) -> None:
    for _ in range(200):
        if bedingung():
            return
        if p.poll() is not None:
            raise RuntimeError(f"{was} ist gestorben:\n{p.stdout.read().decode()}")
        time.sleep(0.05)
    raise RuntimeError(f"{was} kommt nicht hoch")


def port_offen(port: int) -> bool:
    with socket.socket() as s:
        return s.connect_ex(("127.0.0.1", port)) == 0


@contextlib.contextmanager
def laeuft(befehl, cwd, bereit, was):
    p = subprocess.Popen(befehl, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        warte_bis(bereit, p, was)
        yield p
    finally:
        p.terminate()
        with contextlib.suppress(subprocess.TimeoutExpired):
            p.wait(timeout=5)


async def warte_auf(bedingung, frist=15.0) -> bool:
    ende = asyncio.get_running_loop().time() + frist
    while asyncio.get_running_loop().time() < ende:
        if bedingung():
            return True
        await asyncio.sleep(0.02)
    return False


def verbinder_fuer(sockel: str):
    """Wie Caddy: ein Unix-Socket und ein X-Forwarded-For."""
    async def _v():
        from websockets.asyncio.client import unix_connect

        return await unix_connect(
            sockel, "ws://relay/v1/relay",
            additional_headers={"X-Forwarded-For": "192.0.2.7"},
            max_size=1600, ping_interval=None)
    return _v


async def mit_station(url, tmp, tci_port, name, **opt):
    liste = Geraeteliste(tmp / f"geraete-{name}.json")
    st = Station(url, tmp / f"station-{name}.pem", geraete=liste,
                 ziel=Ziel(url=f"ws://127.0.0.1:{tci_port}"), **opt)
    halten = asyncio.create_task(st.halte_offen())
    ok = await warte_auf(lambda: st.lage.angemeldet)
    return st, liste, halten, ok


async def beende(st, halten):
    halten.cancel()
    with contextlib.suppress(asyncio.CancelledError):
        await halten
    for s in list(st._sitzungen.values()):
        await s.schliesse()


async def lauf() -> None:
    rv_port, tci_port = freier_port(), freier_port()
    url = f"ws://127.0.0.1:{rv_port}/"
    with tempfile.TemporaryDirectory() as tmp, \
            tempfile.TemporaryDirectory(prefix="nr", dir="/tmp") as sockdir:
        tmp = Path(tmp)
        sockel = os.path.join(sockdir, "relay.sock")
        geheim = tmp / "relay-secret"
        geheim.write_text(secrets.token_hex(32))
        geheim.chmod(0o600)
        (tmp / "rv.conf").write_text(
            f"[rendezvous]\nlisten = 127.0.0.1:{rv_port}\nstun_urls =\nlog_level = warning\n"
            f"relay_url = wss://rv.invalid/v1/relay\nrelay_secret_file = {geheim}\n")
        (tmp / "relay.conf").write_text(
            f"[relay]\nsocket = {sockel}\nsocket_mode = 0600\nsocket_group =\n"
            f"relay_secret_file = {geheim}\nlog_level = warning\n")

        verbinder = verbinder_fuer(sockel)
        with laeuft([sys.executable, "-m", "nereus_rendezvous", "--config", str(tmp / "rv.conf")],
                    DIENST, lambda: port_offen(rv_port), "der Dienst"), \
             laeuft([sys.executable, "-m", "nereus_relay", "--config", str(tmp / "relay.conf")],
                    DIENST, lambda: os.path.exists(sockel), "der Relay"), \
             laeuft([sys.executable, str(ATTRAPPE), "--port", str(tci_port)],
                    WURZEL, lambda: port_offen(tci_port), "die Attrappe"):

            # ── 1. Nur Web: der Relay ist der einzige Weg ─────────────
            print("1. Netz ohne UDP, mit Relay")
            st, liste, halten, ok = await mit_station(
                url, tmp, tci_port, "a", nur_relais=True,
                relais_verbinder=verbinder, relais_verzoegerung_s=0.3)
            try:
                if not ok:
                    fehler("die Station hat sich nicht angemeldet")
                    return
                tel = Geraeteseite(url, tmp / "tel-a.pem", st.lage.ident, nur_relais=True,
                                   relais_verbinder=verbinder, relais_verzoegerung_s=0.3)
                liste.koppele(tel.schluessel_text, "Pruefstand", "phone")
                empfangen: list = []
                tel.beim_empfang = empfangen.append
                try:
                    await tel.verbinde(frist=25.0)
                    gut("das Geraet kommt durch den Relay herein")
                except Exception as e:
                    fehler(f"durch den Relay kam das Geraet nicht herein: {type(e).__name__} {e}")
                    await tel.schliesse()
                    return
                if not await warte_auf(lambda: len(empfangen) > 0, 10.0):
                    fehler("durch den Relay kam kein TCI")
                else:
                    gut(f"{len(empfangen)} TCI-Nachrichten durch den Relay")
                vorher = len(empfangen)
                tel.sende("vfo:0,0;")
                if await warte_auf(lambda: len(empfangen) > vorher, 5.0):
                    gut("Abfrage hin und Antwort zurueck")
                else:
                    fehler("auf eine Abfrage kam nichts zurueck")
                vorher = len(empfangen)
                tel.sende("start;")
                tel.sende("audio_start:0;")
                if await warte_auf(lambda: any(isinstance(m, (bytes, bytearray))
                                               for m in empfangen[vorher:]), 10.0):
                    gut("Binaerrahmen (Ton) kommen durch")
                else:
                    fehler("kein einziger Binaerrahmen durch den Relay")

                sitzung = next(iter(st._sitzungen.values()), None)
                if sitzung is None or not sitzung.laeuft_ueber_relais():
                    fehler("ICE hat das Paar NICHT ueber die Relay-Steckdose gewaehlt")
                else:
                    gut("ICE laeuft ueber das Relay-Paar (nominiert, nicht nur offen)")
                z = (sitzung.relais.gezaehlt if sitzung and sitzung.relais else {})
                if z.get("hinaus", 0) > 0 and z.get("herein", 0) > 0:
                    gut(f"Relay-Zaehler Station: hinaus {z['hinaus']}, herein {z['herein']}")
                else:
                    fehler(f"Relay-Zaehler stehen still: {z}")
                if tel.relais and tel.relais.gegenseite:
                    gut("der Relay meldet beide Beine da (READY/PEER)")
                else:
                    fehler("der Relay meldete die Gegenseite nicht")
                await tel.schliesse()
            finally:
                await beende(st, halten)

            # ── 2. Gegenprobe: dasselbe Netz, kein Relay-Angebot ──────
            # Das Telefon kennt den Relay nicht (kein Verbinder, die URL
            # waere unerreichbar), die Station ebenso: ohne Relay darf nichts
            # durchkommen. Sonst waere Punkt 1 nicht der Relay gewesen.
            print("2. Gegenprobe: Netz ohne UDP, Relay unerreichbar")

            async def unerreichbar():
                raise ConnectionRefusedError("kein Relay in diesem Netz")

            st, liste, halten, ok = await mit_station(
                url, tmp, tci_port, "b", nur_relais=True,
                relais_verbinder=unerreichbar, relais_verzoegerung_s=0.3)
            try:
                tel = Geraeteseite(url, tmp / "tel-b.pem", st.lage.ident, nur_relais=True,
                                   relais_verbinder=unerreichbar, relais_verzoegerung_s=0.3)
                liste.koppele(tel.schluessel_text, "Pruefstand", "phone")
                try:
                    await tel.verbinde(frist=9.0)
                    fehler("ohne Relay kam das Geraet durch -- Punkt 1 belegt nichts")
                except (asyncio.TimeoutError, RuntimeError, Exception):
                    gut("ohne Relay kommt in diesem Netz nichts durch")
                finally:
                    await tel.schliesse()
            finally:
                await beende(st, halten)

            # ── 3. Ein Token, den der Relay nicht kennt ───────────────
            print("3. Falsches Token")
            bein = Relaisbein("x" * 83, verbinder=verbinder)
            await bein.starte()
            await asyncio.sleep(1.0)
            if bein.ende == "badToken":
                gut("badToken: das Angebot gilt als verbraucht, kein zweiter Versuch")
            else:
                fehler(f"badToken nicht erkannt, Ende: {bein.ende!r}")
            await bein.schliesse()

            # ── 4. Normalbetrieb: der Relay bleibt liegen? ────────────
            print("4. Normalbetrieb mit direktem Weg")
            st, liste, halten, ok = await mit_station(
                url, tmp, tci_port, "c", relais_verbinder=verbinder,
                relais_verzoegerung_s=0.5)
            try:
                tel = Geraeteseite(url, tmp / "tel-c.pem", st.lage.ident,
                                   relais_verbinder=verbinder, relais_verzoegerung_s=0.5)
                liste.koppele(tel.schluessel_text, "Pruefstand", "phone")
                await tel.verbinde(frist=25.0)
                gut("mit direktem Weg kommt das Geraet herein")
                await asyncio.sleep(2.5)
                sitzung = next(iter(st._sitzungen.values()), None)
                if sitzung is None:
                    fehler("die Sitzung ist weg")
                elif sitzung.laeuft_ueber_relais():
                    fehler("ICE hat trotz direktem Weg den Relay gewaehlt")
                else:
                    gut("der direkte Weg gewinnt, der Relay wird nicht gewaehlt")
                if sitzung is not None and sitzung.relais is not None:
                    fehler("das Relay-Bein liegt noch, obwohl die Sitzung anders laeuft")
                else:
                    gut("das Relay-Bein wurde geschlossen und liegt nicht herum")
                await tel.schliesse()
            finally:
                await beende(st, halten)


def main() -> int:
    logging.basicConfig(level=logging.WARNING, format="%(levelname)s %(name)s %(message)s")
    asyncio.run(lauf())
    print()
    print("ALLES GRUEN" if schlecht == 0 else f"{schlecht} Abweichungen")
    return 0 if schlecht == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
