#!/usr/bin/env python3
"""Die Stationsrolle gegen einen echten Rendezvous-Dienst fahren.

Startet den Dienst aus `rendezvous/server/` selbst auf einem freien Port,
meldet eine Station an, holt eine Nummer und prueft die Faelle, die im
Betrieb wirklich vorkommen: ein zweites Anmelden derselben Id verdraengt
das erste, und eine abgerissene Verbindung kommt von allein wieder.

    python3 handfunke/fernzugriff/pruefe-station.py
"""

from __future__ import annotations

import asyncio
import contextlib
import logging
import socket
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from station import FehlerVomDienst, Station  # noqa: E402

WURZEL = Path(__file__).resolve().parents[2]
DIENST = WURZEL / "rendezvous" / "server"
schlecht = 0


def fehler(zeile: str) -> None:
    global schlecht
    schlecht += 1
    print("  FEHLER:", zeile)


def freier_port() -> int:
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


@contextlib.contextmanager
def dienst_laeuft(port: int):
    """Den Dienst starten und am Ende wieder abraeumen."""
    with tempfile.NamedTemporaryFile("w", suffix=".conf", delete=False) as f:
        f.write(f"[rendezvous]\nlisten = 127.0.0.1:{port}\nstun_urls =\nlog_level = warning\n")
        conf = f.name
    p = subprocess.Popen(
        [sys.executable, "-m", "nereus_rendezvous", "--config", conf],
        cwd=DIENST,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    try:
        # Warten, bis der Port wirklich annimmt -- eine feste Wartezeit
        # waere hier die uebliche Quelle wackeliger Laeufe.
        for _ in range(100):
            with socket.socket() as s:
                if s.connect_ex(("127.0.0.1", port)) == 0:
                    break
            if p.poll() is not None:
                raise RuntimeError(f"der Dienst ist gestorben:\n{p.stdout.read().decode()}")
            import time

            time.sleep(0.05)
        else:
            raise RuntimeError("der Dienst nimmt nicht an")
        yield
    finally:
        p.terminate()
        with contextlib.suppress(subprocess.TimeoutExpired):
            p.wait(timeout=5)
        Path(conf).unlink(missing_ok=True)


async def warte_auf(bedingung, frist=10.0, was="etwas"):
    """Auf eine Bedingung warten statt auf eine Uhr."""
    ende = asyncio.get_running_loop().time() + frist
    while asyncio.get_running_loop().time() < ende:
        if bedingung():
            return True
        await asyncio.sleep(0.02)
    fehler(f"{was} kam nicht binnen {frist} s")
    return False


async def lauf() -> None:
    port = freier_port()
    url = f"ws://127.0.0.1:{port}/"
    with tempfile.TemporaryDirectory() as tmp:
        schluessel = Path(tmp) / "station.pem"
        with dienst_laeuft(port):
            st = Station(url, schluessel)
            ident = st.lage.ident
            print(f"Id: {ident}")
            if len(ident) != 26:
                fehler(f"Id ist {len(ident)} Zeichen lang")

            aufgabe = asyncio.create_task(st.halte_offen())
            try:
                await warte_auf(lambda: st.lage.angemeldet, was="die Anmeldung")
                await warte_auf(lambda: st.lage.nummer is not None, was="die Nummer")
                print(f"Nummer: {st.lage.nummer}")
                if st.lage.nummer != 1:
                    fehler(f"die erste Nummer am leeren Dienst ist {st.lage.nummer}, nicht 1")

                # Der Schluessel liegt auf der Platte und ergibt beim
                # naechsten Start dieselbe Id -- sonst muesste jedes
                # Telefon nach jedem Neustart neu gekoppelt werden.
                zweite = Station(url, schluessel)
                if zweite.lage.ident != ident:
                    fehler("derselbe Schluessel ergibt eine andere Id")

                # Ein zweites Anmelden derselben Id verdraengt das erste
                # (Abschnitt 6.2): genau das passiert, wenn Longpath neu
                # startet, waehrend die alte Verbindung noch haengt.
                aufgabe2 = asyncio.create_task(zweite.halte_offen())
                try:
                    await warte_auf(lambda: zweite.lage.angemeldet, was="die zweite Anmeldung")
                    await warte_auf(
                        lambda: not st.lage.angemeldet,
                        was="die Verdraengung der ersten",
                    )
                    if "replaced" not in st.lage.letzter_fehler:
                        fehler(
                            "die verdraengte Station nennt nicht replaced, "
                            f"sondern {st.lage.letzter_fehler!r}"
                        )
                    # Und sie kommt von allein wieder -- und verdraengt
                    # damit die zweite.
                    await warte_auf(
                        lambda: st.lage.angemeldet,
                        frist=15.0,
                        was="die Rueckkehr der ersten",
                    )
                finally:
                    aufgabe2.cancel()
                    with contextlib.suppress(asyncio.CancelledError):
                        await aufgabe2
            finally:
                aufgabe.cancel()
                with contextlib.suppress(asyncio.CancelledError):
                    await aufgabe

        # Ohne Dienst: die Station darf nicht aufgeben, sondern muss
        # weiter klopfen. (Der Dienst ist hier schon abgeraeumt.)
        dritte = Station(url, schluessel)
        aufgabe3 = asyncio.create_task(dritte.halte_offen())
        try:
            await asyncio.sleep(2.0)
            if dritte.lage.angemeldet:
                fehler("ohne Dienst meldet sich die Station trotzdem an")
            if not dritte.lage.letzter_fehler:
                fehler("ohne Dienst nennt die Station keinen Grund")
            if aufgabe3.done():
                fehler("ohne Dienst gibt die Station auf, statt es weiter zu versuchen")
        finally:
            aufgabe3.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await aufgabe3


def main() -> int:
    logging.basicConfig(level=logging.WARNING, format="%(levelname)s %(message)s")
    asyncio.run(lauf())
    print()
    print("ALLES GRUEN" if schlecht == 0 else f"{schlecht} Abweichungen")
    return 0 if schlecht == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
