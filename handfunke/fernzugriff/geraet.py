"""Die Geraeteseite: anklopfen und hereinkommen.

Das ist, was die Webseite spaeter in JavaScript tut -- hier in Python,
damit der ganze Weg ohne Browser gefahren und geprueft werden kann. Die
Reihenfolge ist dieselbe und sie ist Teil des Protokolls:

1. zum Rendezvous verbinden und das `hello` lesen (die nonce daraus geht
   in die Unterschrift),
2. ein WebRTC-Angebot bauen -- **ohne eigene Kandidaten**, die kommen per
   trickle, weil das Angebot sonst auf das Sammeln warten muesste
   (Abschnitt 6.3),
3. `introduce` mit Geraete-Id, Unterschrift und Angebot,
4. die `answer` auswerten; die TURN-Zugangsdaten darin sind der Grund,
   warum ueberhaupt etwas durch ein Mobilfunknetz kommt,
5. Kandidaten tauschen, bis die Verbindung steht,
6. auf dem Datenkanal die Aufforderung der Station unterschreiben,
7. danach TCI sprechen wie im eigenen Netz.

Schritt 6 ist nicht hoeflich, sondern die Tuer: vorher laesst die Station
kein Byte durch.
"""

from __future__ import annotations

import asyncio
import json
import logging
from pathlib import Path
from typing import Any, Callable

import websockets
from aiortc import (
    RTCConfiguration,
    RTCIceServer,
    RTCPeerConnection,
    RTCSessionDescription,
)
from aiortc.sdp import candidate_from_sdp

from kennung import (
    b64url,
    einfuehr_transkript,
    geraete_id,
    lade_schluessel,
    spki_der,
    unb64url,
    unterschreibe,
)
from sitzung import KANAL, anmelde_transkript

log = logging.getLogger("fernzugriff.geraet")


class Geraeteseite:
    """Ein Telefon, das eine Station von aussen erreicht."""

    def __init__(self, dienst_url: str, schluesselpfad: Path, station_id: str,
                 kann: list[str] | None = None) -> None:
        self.url = dienst_url
        # Was dieses Geraet der Station beim Anmelden ueber sich sagt.
        # Ein Pruefstand meldet hier "opus", OHNE einen Dekoder zu haben:
        # fuer eine Messung am Datenkanal zaehlen die Ankunftszeiten der
        # Pakete, nicht ihr Inhalt -- und so entsteht kein Ton, der
        # jemandem im Zimmer auf die Nerven geht.
        self.kann = list(kann or [])
        self.privat = lade_schluessel(Path(schluesselpfad))
        self.oeffentlich = self.privat.public_key()
        self.ident = geraete_id(self.oeffentlich)
        self.station_id = station_id
        self.pc: RTCPeerConnection | None = None
        self.kanal: Any = None
        self.offen = asyncio.Event()
        self.beim_empfang: Callable[[Any], None] | None = None
        # Womit die Aufforderung auf dem Kanal unterschrieben wird. Im
        # Betrieb immer derselbe Schluessel; der Pruefstand setzt hier
        # einen anderen ein, um die zweite Tuer zu treffen, ohne schon an
        # der Einfuehrung zu scheitern.
        self.anmeldeschluessel = self.privat
        self._ws: Any = None

    @property
    def schluessel_text(self) -> str:
        """Der oeffentliche Schluessel, wie er beim Koppeln uebergeben wird."""
        return b64url(spki_der(self.oeffentlich))

    async def _sende(self, nachricht: dict) -> None:
        await self._ws.send(json.dumps(nachricht, separators=(",", ":")))

    async def verbinde(self, frist: float = 30.0) -> None:
        """Den ganzen Weg gehen, bis der Kanal offen und angemeldet ist."""
        async with websockets.connect(self.url, max_size=1 << 20) as ws:
            self._ws = ws
            hallo = json.loads(await ws.recv())
            if hallo.get("type") != "hello":
                raise ValueError(f"erst kam {hallo.get('type')!r}, nicht hello")
            nonce = unb64url(hallo["nonce"])
            stun = list(hallo.get("stun") or [])

            self.pc = RTCPeerConnection(
                RTCConfiguration([RTCIceServer(urls=stun)] if stun else [])
            )
            self.kanal = self.pc.createDataChannel(KANAL)
            self._verdrahte_kanal()

            angebot = await self.pc.createOffer()
            await self.pc.setLocalDescription(angebot)

            await self._sende(
                {
                    "type": "introduce",
                    "id": self.station_id,
                    "device": self.ident,
                    "deviceSignature": unterschreibe(
                        self.privat, einfuehr_transkript(self.station_id, nonce)
                    ),
                    "offer": self.pc.localDescription.sdp,
                }
            )

            await asyncio.wait_for(self._warte_auf_antwort(ws), frist)
            await asyncio.wait_for(self.offen.wait(), frist)
            # Sobald die Sitzung steht, ist die Rendezvous-Verbindung
            # entbehrlich (Abschnitt 6.3) -- die Station bekommt dann
            # `clientLeft` und laesst die Sitzung ausdruecklich stehen.

    async def _warte_auf_antwort(self, ws) -> None:  # noqa: ANN001
        async for roh in ws:
            nachricht = json.loads(roh)
            art = nachricht.get("type")
            if art == "answer":
                await self.pc.setRemoteDescription(
                    RTCSessionDescription(nachricht["answer"], "answer")
                )
                # Die TURN-Zugangsdaten kommen hier an. aiortc kann sie
                # einer schon laufenden Verbindung nicht mehr mitgeben --
                # fuer die Webseite ist das anders, dort bekommt der
                # Browser sie rechtzeitig in der Konfiguration.
                if nachricht.get("turn"):
                    log.info("TURN angeboten: %s", nachricht["turn"].get("urls"))
                return
            if art == "candidate":
                text = nachricht.get("candidate", "")
                if text:
                    k = candidate_from_sdp(text)
                    k.sdpMLineIndex = 0
                    await self.pc.addIceCandidate(k)
            elif art == "error":
                raise RuntimeError(f"{nachricht.get('code')}: {nachricht.get('reason')}")
            elif art == "introduction.end":
                raise RuntimeError(f"die Einfuehrung endete: {nachricht.get('code')}")

    def _verdrahte_kanal(self) -> None:
        @self.kanal.on("message")
        def _(nachricht):  # noqa: ANN001
            if not self.offen.is_set():
                self._antworte_anmeldung(nachricht)
                return
            if self.beim_empfang is not None:
                self.beim_empfang(nachricht)

    def _antworte_anmeldung(self, nachricht: Any) -> None:
        if isinstance(nachricht, (bytes, bytearray)):
            return
        m = json.loads(nachricht)
        if m.get("typ") == "anmelden":
            self.kanal.send(
                json.dumps(
                    {
                        "typ": "anmeldung",
                        "signatur": unterschreibe(
                            self.anmeldeschluessel,
                            anmelde_transkript(unb64url(m["aufforderung"])),
                        ),
                        "kann": self.kann,
                    },
                    separators=(",", ":"),
                )
            )
        elif m.get("typ") == "offen":
            self.offen.set()
        elif m.get("typ") == "fehler":
            raise RuntimeError(f"die Station sagt ab: {m.get('grund')}")

    def sende(self, nachricht: Any) -> None:
        """Eine TCI-Nachricht an Longpath, durch den Kanal."""
        self.kanal.send(nachricht)

    async def schliesse(self) -> None:
        if self.pc is not None:
            await self.pc.close()
