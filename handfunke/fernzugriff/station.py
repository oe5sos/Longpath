"""Die Stationsrolle am Rendezvous: anmelden und erreichbar bleiben.

Massgeblich ist `docs/architecture/2026-09-23-rendezvous-v1.md` (Abschnitte
3, 5, 6.1, 6.2 und 6.5). Wo dieses Modul und das Dokument sich
widersprechen, hat das Dokument recht und dieses Modul den Fehler.

Was hier steht, ist die eine Haelfte des Wegs von aussen: die Station
meldet sich beim Rendezvous an und haelt die Verbindung. Was sie mit einer
Einfuehrung tut, wenn eine kommt, steht noch nicht hier -- das ist der
naechste Schritt.

**Warum die Verbindung offen bleibt.** Der Dienst kennt keine Adressen; er
erreicht die Station nur ueber die Verbindung, die sie selbst geoeffnet
hat. Faellt sie weg, ist die Station fuer jedes Telefon "offline"
(Abschnitt 6.4), auch wenn Longpath laeuft. Darum: wiederverbinden, immer,
mit wachsender Pause.
"""

from __future__ import annotations

import asyncio
import json
import logging
import random
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable

import websockets

from kennung import (
    lade_schluessel,
    registrier_transkript,
    rendezvous_id,
    spki_der,
    unb64url,
    unterschreibe,
    b64url,
)

log = logging.getLogger("fernzugriff.station")

# Wartezeiten beim Wiederverbinden. Die erste Pause ist kurz, damit ein
# Netzwechsel am Mac (WLAN -> Kabel) keine Minute kostet; die letzte ist
# lang genug, dass ein abgeschalteter Dienst nicht dauernd angeklopft wird.
PAUSEN_S = (1, 2, 5, 10, 30, 60)

# Der Dienst schickt sein hello sofort; kommt binnen dieser Zeit keins,
# haengt etwas davor (ein Portal, ein Proxy) und die Verbindung taugt nicht.
HELLO_FRIST_S = 10.0


@dataclass
class Lage:
    """Was die Station gerade ueber sich weiss -- fuer die Anzeige."""

    verbunden: bool = False
    angemeldet: bool = False
    ident: str = ""
    nummer: int | None = None
    stun: list[str] = field(default_factory=list)
    nonce: bytes = b""
    letzter_fehler: str = ""


class Station:
    """Haelt eine Anmeldung am Rendezvous offen.

    `beim_wechsel` wird nach jeder Aenderung der Lage gerufen (im
    Ereignisfaden), damit eine Anzeige mitlaeuft.
    """

    def __init__(
        self,
        dienst_url: str,
        schluesselpfad: Path,
        beim_wechsel: Callable[[Lage], None] | None = None,
        nameplate: bool = True,
    ) -> None:
        self.url = dienst_url
        self.privat = lade_schluessel(Path(schluesselpfad))
        self.oeffentlich = self.privat.public_key()
        self.lage = Lage(ident=rendezvous_id(self.oeffentlich))
        self._beim_wechsel = beim_wechsel
        self._will_nameplate = nameplate
        self._ws: Any = None

    # -- Anzeige ---------------------------------------------------------

    def _melde(self) -> None:
        if self._beim_wechsel is not None:
            try:
                self._beim_wechsel(self.lage)
            except Exception:
                log.exception("Anzeige hat geworfen")

    # -- Draht -----------------------------------------------------------

    async def _sende(self, nachricht: dict) -> None:
        text = json.dumps(nachricht, separators=(",", ":"))
        log.debug("-> %s", text)
        await self._ws.send(text)

    async def _lies(self) -> dict:
        roh = await self._ws.recv()
        if isinstance(roh, (bytes, bytearray)):
            raise ValueError("der Dienst spricht Text, nicht Binaeres")
        log.debug("<- %s", roh)
        nachricht = json.loads(roh)
        if not isinstance(nachricht, dict) or not isinstance(nachricht.get("type"), str):
            raise ValueError(f"keine Nachricht: {roh[:120]}")
        return nachricht

    # -- Anmeldung -------------------------------------------------------

    async def _melde_an(self) -> None:
        """hello -> register -> challenge -> prove -> registered (6.1, 6.2)."""
        hallo = await asyncio.wait_for(self._lies(), HELLO_FRIST_S)
        if hallo["type"] != "hello":
            raise ValueError(f"erst kam {hallo['type']!r}, nicht hello")
        if hallo.get("version") != 1:
            # Fassung 1 ist, was wir sprechen. Eine hoehere Fassung muss
            # abwaertsvertraeglich sein (Abschnitt 11), eine niedrigere
            # gibt es nicht.
            log.warning("der Dienst spricht Fassung %r, wir Fassung 1", hallo.get("version"))
        self.lage.nonce = unb64url(hallo["nonce"])
        self.lage.stun = list(hallo.get("stun") or [])
        self.lage.verbunden = True
        self._melde()

        await self._sende(
            {
                "type": "register",
                "id": self.lage.ident,
                "publicKey": b64url(spki_der(self.oeffentlich)),
            }
        )
        antwort = await self._lies()
        if antwort["type"] == "error":
            raise FehlerVomDienst(antwort)
        if antwort["type"] != "challenge":
            raise ValueError(f"auf register kam {antwort['type']!r}, nicht challenge")

        nonce = unb64url(antwort["nonce"])
        await self._sende(
            {
                "type": "prove",
                "signature": unterschreibe(self.privat, registrier_transkript(nonce)),
            }
        )
        antwort = await self._lies()
        if antwort["type"] == "error":
            raise FehlerVomDienst(antwort)
        if antwort["type"] != "registered":
            raise ValueError(f"auf prove kam {antwort['type']!r}, nicht registered")
        if antwort.get("id") != self.lage.ident:
            raise ValueError("der Dienst hat eine andere Id bestaetigt")

        self.lage.angemeldet = True
        self.lage.letzter_fehler = ""
        self._melde()
        log.info("angemeldet als %s", self.lage.ident)

        if self._will_nameplate:
            await self._sende({"type": "nameplate.claim"})

    # -- Betrieb ---------------------------------------------------------

    async def _laufe(self) -> None:
        """Eine Verbindung von der Anmeldung bis zu ihrem Ende."""
        async with websockets.connect(self.url, max_size=1 << 20) as ws:
            self._ws = ws
            try:
                await self._melde_an()
                async for _ in self._nachrichten():
                    pass
            finally:
                self._ws = None

    async def _nachrichten(self):
        """Alles, was nach der Anmeldung kommt."""
        while True:
            nachricht = await self._lies()
            art = nachricht["type"]
            if art == "nameplate":
                self.lage.nummer = nachricht["nameplate"]
                self._melde()
                log.info("Nummer %s", self.lage.nummer)
            elif art == "nameplate.released":
                self.lage.nummer = None
                self._melde()
            elif art == "error":
                # Ein Fehler, der die Verbindung stehen laesst (Abschnitt 7),
                # ist eine Absage an EINE Sache, nicht an die Anmeldung.
                fehler = FehlerVomDienst(nachricht)
                self.lage.letzter_fehler = str(fehler)
                self._melde()
                log.warning("%s", fehler)
                if fehler.schliesst:
                    raise fehler
            else:
                # Eine Art, die wir nicht kennen, wird mitgeschrieben und
                # uebergangen -- so darf der Dienst spaeter dazulernen
                # (Abschnitt 5.1).
                log.info("uebergangen: %s", art)
            yield nachricht

    async def halte_offen(self) -> None:
        """Laeuft, bis man sie abbricht: anmelden, halten, wiederverbinden."""
        versuch = 0
        while True:
            try:
                await self._laufe()
                grund = "die Verbindung ging zu"
            except asyncio.CancelledError:
                raise
            except FehlerVomDienst as e:
                grund = str(e)
            except Exception as e:  # Netz, TLS, JSON, alles
                grund = f"{type(e).__name__}: {e}"

            war_angemeldet = self.lage.angemeldet
            self.lage.verbunden = False
            self.lage.angemeldet = False
            self.lage.nummer = None
            self.lage.letzter_fehler = grund
            self._melde()

            # Eine Verbindung, die stand, faengt mit der kurzen Pause neu
            # an: sie ist wahrscheinlich an einem Netzwechsel gestorben,
            # nicht an einem kaputten Dienst.
            versuch = 0 if war_angemeldet else min(versuch + 1, len(PAUSEN_S) - 1)
            pause = PAUSEN_S[versuch]
            # Streuung, damit nicht alle Stationen einer Stoerung im
            # Gleichschritt wiederkommen.
            pause += random.uniform(0, pause * 0.25)
            log.info("weg (%s); in %.1f s wieder", grund, pause)
            await asyncio.sleep(pause)


class FehlerVomDienst(Exception):
    """Ein `error` vom Dienst (Abschnitt 7)."""

    # Die Kennungen, nach denen der Dienst die Verbindung schliesst.
    SCHLIESSEND = {
        "protocolError",
        "proofFailed",
        "timeout",
        "idle",
        "replaced",
        "overloaded",
        "tooManyConnections",
        "shuttingDown",
    }

    def __init__(self, nachricht: dict) -> None:
        self.code = nachricht.get("code", "?")
        self.grund = nachricht.get("reason", "")
        self.wieder_in_ms = nachricht.get("retryAfterMs", 0)
        self.schliesst = self.code in self.SCHLIESSEND
        super().__init__(f"{self.code}: {self.grund}")
