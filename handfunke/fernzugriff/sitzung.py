"""Eine Sitzung von draussen: der Datenkanal und was durch ihn laeuft.

Eine Sitzung entsteht aus einer angenommenen Einfuehrung (Abschnitt 6.3).
Sie ist eine WebRTC-Verbindung zwischen Telefon und Station; ICE sucht
sich den Weg selbst -- im eigenen WLAN direkt, von draussen ueber STUN
oder, wo auch das nicht durchkommt, ueber TURN.

Durch den Datenkanal laeuft **genau das, was die App heute im eigenen Netz
auf ihrem WebSocket spricht**: TCI, Text wie Binaeres, unveraendert. Die
Station haelt dafuer einen eigenen WebSocket zu Longpath auf 127.0.0.1 und
reicht beide Richtungen durch.

**Die zweite Pruefung, und warum sie nicht fehlen darf.** Eine
angenommene Einfuehrung ist keine Anmeldung (Abschnitt 4.4): der Dienst
waehlt die nonce, sieht jede Unterschrift, und eine Verbindung behaelt
ihre nonce fuer mehrere Einfuehrungen. Wer nur darauf baut, laesst jeden
herein, der eine Einfuehrung mitgehoert hat. Darum fragt die Station auf
dem offenen Kanal noch einmal -- mit einem frischen Wert, den sie selbst
wuerfelt -- und laesst **vor der Antwort kein einziges TCI-Byte** durch.

Das ist zugleich die Stelle, an der Longpaths eigene Freigabe NICHT
ausgehebelt werden darf: Longpath verlangt von Verbindungen aus dem Netz
ein Token, von Loopback nie, und diese Bruecke kommt von Loopback. Was
frueher die alte TCI-Bruecke falsch machte (jeder, der sie erreichte, war
drin), faengt hier diese Pruefung ab -- und nur sie. Wer an ihr etwas
lockert, macht die Station offen.
"""

from __future__ import annotations

import asyncio
import json
import logging
import secrets
from dataclasses import dataclass
from typing import Any

import websockets
from aiortc import (
    RTCConfiguration,
    RTCIceServer,
    RTCPeerConnection,
    RTCSessionDescription,
)

from geraete import Geraet, Geraeteliste
from kennung import b64url, pruefe_unterschrift, unb64url
from ton import Umkodierer

log = logging.getLogger("fernzugriff.sitzung")

# Was die Station unterschreiben laesst, bevor etwas durchgeht. Eigenes
# Praefix, damit eine Unterschrift von hier nirgendwo sonst gilt.
PRAEFIX_ANMELDUNG = b"Longpath fernzugriff anmeldung v1\n"

# Wie lange die Anmeldung auf dem offenen Kanal dauern darf.
ANMELDE_FRIST_S = 15.0

# Der Kanal heisst so; ein anderer wird nicht angenommen.
KANAL = "longpath-tci"


def anmelde_transkript(aufforderung: bytes) -> bytes:
    if len(aufforderung) != 32:
        raise ValueError("die Aufforderung ist nicht 32 Byte lang")
    return PRAEFIX_ANMELDUNG + aufforderung


@dataclass
class Ziel:
    """Wohin die Bruecke spricht -- Longpaths TCI-Server."""

    url: str = "ws://127.0.0.1:50001"
    token: str = ""


class Sitzung:
    """Eine Verbindung zu einem Telefon, von der Einfuehrung bis zum Ende."""

    def __init__(
        self,
        geraet: Geraet,
        ziel: Ziel,
        stun: list[str],
        turn: dict | None,
        geraeteliste: Geraeteliste | None = None,
    ) -> None:
        self.geraet = geraet
        self.ziel = ziel
        self.geraeteliste = geraeteliste
        # Der Umkodierer entsteht erst, wenn das Geraet beim Anmelden
        # sagt, dass es Opus kann. Wer das nicht sagt, bekommt den
        # Tonstrom unveraendert -- im eigenen WLAN ist mu-law voellig in
        # Ordnung, und ein Geraet ohne sicheren Kontext hat gar keinen
        # Dekoder (siehe ton.py).
        self.ton: Umkodierer | None = None
        self.pc = RTCPeerConnection(RTCConfiguration(_eisserver(stun, turn)))
        self.angemeldet = False
        self._aufforderung = secrets.token_bytes(32)
        self._kanal: Any = None
        self._longpath: Any = None
        self._pumpe: asyncio.Task | None = None
        self._zu = asyncio.Event()
        self._verdrahte()

    # -- Aufbau ----------------------------------------------------------

    def _verdrahte(self) -> None:
        @self.pc.on("datachannel")
        def _(kanal):  # noqa: ANN001
            if kanal.label != KANAL:
                log.warning("Kanal %r abgewiesen", kanal.label)
                kanal.close()
                return
            self._kanal = kanal
            self._nimm_kanal(kanal)

        @self.pc.on("connectionstatechange")
        async def _():
            log.info("Verbindung: %s", self.pc.connectionState)
            if self.pc.connectionState in ("failed", "closed"):
                self._zu.set()

    def _nimm_kanal(self, kanal) -> None:  # noqa: ANN001
        def fordere_auf() -> None:
            # Die Aufforderung geht sofort hinaus; bis die Antwort da ist,
            # wird nichts weitergereicht.
            kanal.send(
                json.dumps(
                    {"typ": "anmelden", "aufforderung": b64url(self._aufforderung)},
                    separators=(",", ":"),
                )
            )
            asyncio.ensure_future(self._anmeldefrist())

        # Ein Kanal, den die Gegenseite eroeffnet hat, ist beim
        # `datachannel`-Ereignis in aiortc SCHON offen -- "open" feuert
        # dann nie mehr. Wer sich nur darauf verlaesst, schickt die
        # Aufforderung nicht ab und wartet bis zur Frist auf eine Antwort,
        # die niemand geben kann. (Genau so lag es beim ersten Lauf.)
        if kanal.readyState == "open":
            fordere_auf()
        else:
            kanal.on("open")(fordere_auf)

        @kanal.on("message")
        def _(nachricht):  # noqa: ANN001
            if not self.angemeldet:
                asyncio.ensure_future(self._pruefe_anmeldung(nachricht))
                return
            asyncio.ensure_future(self._nach_longpath(nachricht))

        @kanal.on("close")
        def _():
            self._zu.set()

    async def _anmeldefrist(self) -> None:
        await asyncio.sleep(ANMELDE_FRIST_S)
        if not self.angemeldet:
            log.warning("keine Anmeldung binnen %.0f s -- zu", ANMELDE_FRIST_S)
            await self.schliesse()

    async def _pruefe_anmeldung(self, nachricht: Any) -> None:
        """Die einzige Tuer. Alles, was nicht durchkommt, beendet die Sitzung."""
        if isinstance(nachricht, (bytes, bytearray)):
            log.warning("Anmeldung kam binaer")
            await self.schliesse()
            return
        try:
            m = json.loads(nachricht)
            signatur = m["signatur"]
        except Exception:
            log.warning("Anmeldung unlesbar")
            await self.schliesse()
            return

        kann = m.get("kann") or []
        oeff = self.geraet.oeffentlich()
        if not pruefe_unterschrift(oeff, anmelde_transkript(self._aufforderung), signatur):
            # Kein zweiter Versuch auf derselben Verbindung: sonst waere
            # die Aufforderung ein Rateziel.
            log.warning("Anmeldung von %s stimmt nicht", self.geraet.ident[:8])
            await self.schliesse()
            return

        try:
            await self._oeffne_longpath()
        except Exception as e:
            log.error("Longpath nicht erreichbar: %s", e)
            self._sende_kanal(json.dumps({"typ": "fehler", "grund": "kein Longpath"}))
            await self.schliesse()
            return

        if "opus" in kann:
            self.ton = Umkodierer()
            log.info("%s kann Opus -- der Tonstrom wird umkodiert", self.geraet.name)

        self.angemeldet = True
        if self.geraeteliste is not None:
            self.geraeteliste.gesehen(self.geraet.ident)
        self._sende_kanal(json.dumps(
            {"typ": "offen", "ton": "opus" if self.ton else "roh"},
            separators=(",", ":")))
        log.info("%s ist drin", self.geraet.name)

    # -- Die Bruecke -----------------------------------------------------

    async def _oeffne_longpath(self) -> None:
        url = self.ziel.url
        if self.ziel.token:
            # Longpath verlangt das Token nur von Verbindungen aus dem
            # Netz; ueber Loopback nie. Wir schicken es trotzdem mit, wenn
            # eins eingestellt ist -- dann laesst sich die Bruecke auch auf
            # einen anderen Rechner stellen, ohne dass hier etwas fehlt.
            trenner = "&" if "?" in url else "?"
            url = f"{url}{trenner}token={self.ziel.token}"
        self._longpath = await asyncio.wait_for(
            websockets.connect(url, max_size=None), timeout=5.0
        )
        if self.ton is not None:
            # Opus aus mu-law zu rechnen waere doppelt verlustbehaftet,
            # also int16 anfordern -- ueber Loopback kostet das nichts.
            #
            # Das verstellt NICHTS fuer andere: in Longpath gehoert das
            # Tonformat der Sitzung, nicht dem Programm (TciServer.cpp,
            # "Das Format gehoert in Longpath der SITZUNG"). Ein
            # gleichzeitig laufendes WSJT-X und Martins Seite im eigenen
            # Netz behalten ihres.
            await self._longpath.send("audio_stream_sample_type:int16;")
        self._pumpe = asyncio.ensure_future(self._von_longpath())

    async def _von_longpath(self) -> None:
        """Alles, was Longpath sagt, in den Kanal -- den Ton kleingerechnet."""
        try:
            async for nachricht in self._longpath:
                if self.ton is not None and isinstance(nachricht, (bytes, bytearray)):
                    neu = self.ton.rahmen(bytes(nachricht))
                    if neu is not None:
                        # Ein Eingangsrahmen ergibt null, einen oder
                        # mehrere Opus-Rahmen: Opus arbeitet in festen
                        # Bloecken, der Tonstrom kommt in anderen.
                        for r in neu:
                            self._sende_kanal(r)
                        continue
                self._sende_kanal(nachricht)
        except Exception as e:
            log.info("Longpath weg: %s", e)
        finally:
            self._zu.set()

    async def _nach_longpath(self, nachricht: Any) -> None:
        if self._longpath is None:
            return
        try:
            await self._longpath.send(nachricht)
        except Exception as e:
            log.info("an Longpath gescheitert: %s", e)
            self._zu.set()

    def _sende_kanal(self, nachricht: Any) -> None:
        kanal = self._kanal
        if kanal is None or kanal.readyState != "open":
            return
        try:
            kanal.send(nachricht)
        except Exception as e:
            log.info("in den Kanal gescheitert: %s", e)
            self._zu.set()

    # -- Ende ------------------------------------------------------------

    async def warte_auf_ende(self) -> None:
        await self._zu.wait()
        await self.schliesse()

    async def schliesse(self) -> None:
        self._zu.set()
        if self._pumpe is not None:
            self._pumpe.cancel()
            self._pumpe = None
        if self._longpath is not None:
            try:
                await self._longpath.close()
            except Exception:
                pass
            self._longpath = None
        try:
            await self.pc.close()
        except Exception:
            pass


def _eisserver(stun: list[str], turn: dict | None) -> list[RTCIceServer]:
    """Woher ICE seine Wege nimmt.

    `stun` kommt aus dem hello des Dienstes, `turn` aus `credentials` nach
    der eigenen Antwort (Abschnitt 8). Ohne TURN bleibt nur, was direkt
    oder ueber STUN erreichbar ist -- in vielen Mobilfunknetzen also
    nichts.
    """
    server = [RTCIceServer(urls=list(stun))] if stun else []
    if turn and turn.get("urls"):
        server.append(
            RTCIceServer(
                urls=list(turn["urls"]),
                username=turn.get("username"),
                credential=turn.get("password"),
            )
        )
    return server


async def beantworte(
    angebot_sdp: str,
    geraet: Geraet,
    ziel: Ziel,
    stun: list[str],
    turn: dict | None,
    geraeteliste: Geraeteliste | None = None,
) -> tuple[Sitzung, str]:
    """Ein Angebot annehmen und die Antwort zurueckgeben (6.3).

    Die Antwort geht ohne eigene Kandidaten hinaus; beide Seiten trickeln
    sie danach einzeln durch den Dienst.
    """
    sitzung = Sitzung(geraet, ziel, stun, turn, geraeteliste)
    await sitzung.pc.setRemoteDescription(RTCSessionDescription(angebot_sdp, "offer"))
    antwort = await sitzung.pc.createAnswer()
    await sitzung.pc.setLocalDescription(antwort)
    return sitzung, sitzung.pc.localDescription.sdp
