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
import os
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
from ton import Umkodierer, lies_kopf
from relais import Relaisbein, kandidat_text, verbinde_url

log = logging.getLogger("fernzugriff.sitzung")

# Was die Station unterschreiben laesst, bevor etwas durchgeht. Eigenes
# Praefix, damit eine Unterschrift von hier nirgendwo sonst gilt.
PRAEFIX_ANMELDUNG = b"Longpath fernzugriff anmeldung v1\n"

# Wie lange die Anmeldung auf dem offenen Kanal dauern darf.
ANMELDE_FRIST_S = 15.0

# Der Kanal heisst so; ein anderer wird nicht angenommen.
KANAL = "longpath-tci"

# Der zweite Kanal, nur fuer die Opus-Bloecke der Station zum Telefon:
# ungeordnet und ohne Wiederholung. Ein verlorener Block ist 20 ms Ton, die
# Wiederholung kaeme Hunderte Millisekunden zu spaet -- und auf dem
# geordneten Kanal hielte sie alle folgenden Bloecke (und TCI-Befehle) auf.
# Hier kommt NUR Ton von der Station zum Telefon durch; was das Telefon
# dort schickt, wird verworfen, und vor der Anmeldung auf KANAL gibt es
# auch hier nichts (die Tuer bleibt die einzige).
KANAL_TON = "longpath-ton"

# Abschaltbar, damit sich die Gegenprobe gegen die Fassung mit einem Kanal
# fahren laesst (pruefe-tonkanal.py). Im Betrieb immer an.
def tonkanal_an() -> bool:
    return os.environ.get("LONGPATH_TONKANAL", "1") != "0"

# Die Taktung der Tonbloecke abschaltbar, damit sich die Gegenprobe
# gegen die alte Fassung fahren laesst: ein guter Wert ohne Gegenprobe
# belegt nichts. Im Betrieb immer an; nur der Pruefstand setzt
# LONGPATH_TONTAKT=0.
# Wie lange ICE den direkten Weg und TURN allein versuchen darf, bevor
# der Relay als letzter Weg dazukommt. Er hat die niedrigste Prioritaet,
# aber aioice nominiert das ERSTE gelungene Paar -- liefe der Relay vom
# ersten Augenblick an mit, koennte er einen noch laufenden direkten Weg
# schlagen. Der Relay-WebSocket selbst wird sofort geoeffnet (12.1: kein
# Warten auf die Kandidatensammlung); nur der Kandidat kommt spaeter.
RELAIS_VERZOEGERUNG_S = 3.0

TAKTUNG = os.environ.get("LONGPATH_TONTAKT", "1") != "0"


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
        relais_verzoegerung_s: float = RELAIS_VERZOEGERUNG_S,
    ) -> None:
        self.geraet = geraet
        self._relais_verzoegerung = relais_verzoegerung_s
        self.relais: Relaisbein | None = None
        self._relais_aufgabe: asyncio.Task | None = None
        self.ziel = ziel
        self.geraeteliste = geraeteliste
        # Der Umkodierer entsteht erst, wenn das Geraet beim Anmelden
        # sagt, dass es Opus kann. Wer das nicht sagt, bekommt den
        # Tonstrom unveraendert -- im eigenen WLAN ist mu-law voellig in
        # Ordnung, und ein Geraet ohne sicheren Kontext hat gar keinen
        # Dekoder (siehe ton.py).
        self.ton: Umkodierer | None = None
        self._tontakt: asyncio.Task | None = None
        self._tonschlange: asyncio.Queue | None = None
        self._tonkanal: Any = None
        self._ton_eigener = 0       # Bloecke, die ueber KANAL_TON gingen
        # PRUEFHAKEN: wenn eine Liste, kommt jeder ausgehende Opus-Block mit
        # seiner Sendezeit hinein -- fuer die Laufzeitmessung im Pruefstand.
        self.sendelog: list | None = None
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
            if kanal.label == KANAL_TON and self._tonkanal is None:
                self._tonkanal = kanal
                # Vom Telefon wird hier nichts angenommen.
                kanal.on("message")(lambda _m: None)
                return
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

        # ZUERST den Umkodierer, DANN die Verbindung zu Longpath: in
        # `_oeffne_longpath` wird int16 angefordert und die Tonpumpe
        # gestartet, und beides haengt an `self.ton`. In der anderen
        # Reihenfolge lief alles scheinbar richtig -- die Station meldete
        # sogar "kann Opus" --, aber der Ton kam weiter als Float32
        # stereo durch, ungetaktet und unkodiert. Am 2026-10-28 so
        # gemessen: 170 Rahmen "tci strom1 typ3 kan2", kein einziges
        # Opus-Paket.
        if "opus" in kann:
            self.ton = Umkodierer()
            log.info("%s kann Opus -- der Tonstrom wird umkodiert", self.geraet.name)

        try:
            await self._oeffne_longpath()
        except Exception as e:
            log.error("Longpath nicht erreichbar: %s", e)
            self._sende_kanal(json.dumps({"typ": "fehler", "grund": "kein Longpath"}))
            await self.schliesse()
            return

        self.angemeldet = True
        if self.geraeteliste is not None:
            self.geraeteliste.gesehen(self.geraet.ident)
        self._sende_kanal(json.dumps(
            {"typ": "offen", "ton": "opus" if self.ton else "roh"},
            separators=(",", ":")))
        log.info("%s ist drin", self.geraet.name)

    # -- Die Bruecke -----------------------------------------------------

    async def _tonpumpe(self) -> None:
        """Die Opus-Bloecke im Takt ihrer eigenen Dauer ausgeben.

        **Warum das noetig ist, gemessen statt vermutet** (2026-10-28, im
        Browser an der Ankunftsstelle): Soll waren 20 ms Abstand. Von
        1044 Paketen kamen **374 im selben Augenblick wie das vorige**
        (unter 2 ms) und 151 nach einer Luecke von ueber 50 ms; der
        Mittelwert lag bei 46,6 ms. Die Bloecke kamen also in Zweier- und
        Dreierschueben alle ~46 ms statt einzeln alle 20.
        Der Vorrat der Tonkette reicht dafuer nicht, und genau das war
        das Stocken -- nicht die Datenmenge, wie ich zuerst vermutet
        hatte (bei einem Zwanzigstel der Daten stockte es unveraendert).

        Die Pumpe kostet keine zusaetzliche Verzoegerung: sie gibt jeden
        Block zu der Zeit aus, die er ohnehin darstellt. Was sie aufholen
        muss, holt sie auf -- faellt die Schlange zurueck, geht der
        naechste Block sofort hinaus statt nach einer weiteren Pause.
        """
        schlange = self._tonschlange
        naechste = None
        while True:
            rahmen = await schlange.get()
            if rahmen is None:
                return
            jetzt = asyncio.get_running_loop().time()
            if naechste is None or jetzt > naechste + 0.5:
                # Erster Block, oder die Uhr ist weit weggelaufen (eine
                # Pause im Strom): neu aufsetzen statt einen Schwall
                # nachzuschieben.
                naechste = jetzt
            else:
                warten = naechste - jetzt
                if warten > 0:
                    await asyncio.sleep(warten)
            self._sende_ton(rahmen)
            # Die Dauer steht im Block selbst: Werte durch Abtastrate.
            kopf = lies_kopf(rahmen)
            dauer = (kopf[1] / kopf[0]) if kopf and kopf[0] else 0.02
            naechste += dauer

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
            #
            # Das steht BEWUSST ausserhalb des Taktungsblocks darunter:
            # haengt es mit daran, bekommt die Gegenprobe
            # (LONGPATH_TONTAKT=0) Float32 statt int16 und damit gar
            # keine Opus-Pakete -- gemessen am 2026-10-28, zweimal
            # "kein einziges Tonpaket" fuer die alte Fassung.
            await self._longpath.send("audio_stream_sample_type:int16;")
        if self.ton is not None and TAKTUNG:
            self._tonschlange = asyncio.Queue()
            self._tontakt = asyncio.ensure_future(self._tonpumpe())
        self._pumpe = asyncio.ensure_future(self._von_longpath())

    async def _von_longpath(self) -> None:
        """Alles, was Longpath sagt, in den Kanal -- den Ton kleingerechnet."""
        try:
            async for nachricht in self._longpath:
                if self.ton is not None and isinstance(nachricht, (bytes, bytearray)):
                    neu = self.ton.rahmen(bytes(nachricht))
                    if neu is not None:
                        # Ein Eingangsrahmen ergibt null, einen oder
                        # mehrere Opus-Bloecke: Opus arbeitet in festen
                        # Bloecken, der Tonstrom kommt in anderen. Sie
                        # gehen NICHT sofort hinaus, sondern in den Takt
                        # (siehe _tonpumpe).
                        for r in neu:
                            if self._tonschlange is not None:
                                self._tonschlange.put_nowait(r)
                            else:
                                self._sende_ton(r)     # alte Fassung
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

    @property
    def ton_ueber_eigenen_kanal(self) -> bool:
        return self._ton_eigener > 0

    def _sende_ton(self, rahmen: bytes) -> None:
        """Ein Opus-Block: auf den eigenen Kanal, wenn es ihn gibt und er offen ist."""
        if self.sendelog is not None:
            self.sendelog.append((asyncio.get_running_loop().time(), bytes(rahmen)))
        k = self._tonkanal
        if tonkanal_an() and self.angemeldet and k is not None and k.readyState == "open":
            try:
                k.send(rahmen)
                self._ton_eigener += 1
                return
            except Exception as e:
                log.info("auf den Tonkanal gescheitert: %s", e)
        self._sende_kanal(rahmen)

    def _sende_kanal(self, nachricht: Any) -> None:
        kanal = self._kanal
        if kanal is None or kanal.readyState != "open":
            return
        try:
            kanal.send(nachricht)
        except Exception as e:
            log.info("in den Kanal gescheitert: %s", e)
            self._zu.set()

    # -- Relay (Abschnitt 12) --------------------------------------------

    async def relais_an(self, url: str, token: str, verbinder=None) -> None:
        """Das Relay-Angebot annehmen. Das Token wird nie geloggt."""
        if self.relais is not None:
            return
        bein = Relaisbein(token, url=url, verbinder=verbinder)
        await bein.starte()
        self.relais = bein
        self._relais_aufgabe = asyncio.ensure_future(self._fuehre_relais())

    async def _fuehre_relais(self) -> None:
        """Kandidat spaet einhaengen; Bein schliessen, wenn es nicht traegt."""
        try:
            await asyncio.sleep(self._relais_verzoegerung)
            if self._zu.is_set() or self.pc.connectionState in ("connected", "closed", "failed"):
                if self.pc.connectionState == "connected":
                    await self._schliesse_relais("anderer Weg steht")
                return
            from aiortc.sdp import candidate_from_sdp

            k = candidate_from_sdp(kandidat_text(self.relais.adresse)[len("candidate:"):])
            k.sdpMLineIndex = 0
            await self.pc.addIceCandidate(k)
            log.info("Relay als letzter Weg angeboten")
            # Steht die Verbindung, und lief sie NICHT ueber den Relay,
            # braucht das Bein keiner mehr.
            while not self._zu.is_set():
                await asyncio.sleep(0.25)
                if self.pc.connectionState == "connected":
                    break
            if not self._zu.is_set() and not self.laeuft_ueber_relais():
                await self._schliesse_relais("Sitzung laeuft anders")
        except asyncio.CancelledError:
            raise
        except Exception as e:
            log.info("Relay-Kandidat gescheitert: %s", e)

    def laeuft_ueber_relais(self) -> bool:
        """Wahr, wenn ICE das Paar ueber die Relay-Steckdose gewaehlt hat."""
        if self.relais is None:
            return False
        try:
            ice = self.pc.sctp.transport.transport._connection
            for paar in ice._nominated.values():
                if (paar.remote_candidate.host, paar.remote_candidate.port) == tuple(self.relais.adresse):
                    return True
        except Exception:
            pass
        return False

    async def _schliesse_relais(self, grund: str) -> None:
        if self.relais is not None:
            log.info("Relay-Bein zu: %s", grund)
            await self.relais.schliesse()
            self.relais = None

    # -- Ende ------------------------------------------------------------

    async def warte_auf_ende(self) -> None:
        await self._zu.wait()
        await self.schliesse()

    async def schliesse(self) -> None:
        self._zu.set()
        if self._pumpe is not None:
            self._pumpe.cancel()
            self._pumpe = None
        if self._tontakt is not None:
            self._tontakt.cancel()
            self._tontakt = None
        if self._relais_aufgabe is not None:
            self._relais_aufgabe.cancel()
            self._relais_aufgabe = None
        if self.relais is not None:
            await self.relais.schliesse()
            self.relais = None
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
    nur_relais: bool = False,
    relais_verzoegerung_s: float = RELAIS_VERZOEGERUNG_S,
) -> tuple[Sitzung, str]:
    """Ein Angebot annehmen und die Antwort zurueckgeben (6.3).

    Die Antwort geht ohne eigene Kandidaten hinaus; beide Seiten trickeln
    sie danach einzeln durch den Dienst.
    """
    sitzung = Sitzung(geraet, ziel, stun, turn, geraeteliste, relais_verzoegerung_s)
    if nur_relais:
        angebot_sdp = ohne_kandidaten(angebot_sdp)
    await sitzung.pc.setRemoteDescription(RTCSessionDescription(angebot_sdp, "offer"))
    antwort = await sitzung.pc.createAnswer()
    await sitzung.pc.setLocalDescription(antwort)
    antwort_sdp = sitzung.pc.localDescription.sdp
    if nur_relais:
        antwort_sdp = ohne_kandidaten(antwort_sdp)
    return sitzung, antwort_sdp


def ohne_kandidaten(sdp: str) -> str:
    """PRUEFHAKEN: alle Kandidaten aus einer SDP nehmen.

    Nur der Pruefstand benutzt das, um ein Netz nachzustellen, in dem
    weder direkt noch ueber UDP etwas durchgeht -- der Relay ist dann der
    einzige Weg. Im Betrieb wird es nie gerufen.
    """
    return "".join(
        z + "\r\n" for z in sdp.splitlines()
        if not z.startswith(("a=candidate:", "a=end-of-candidates"))
    )
