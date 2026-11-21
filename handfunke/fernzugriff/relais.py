"""Der WebSocket-Relay, Endenseite: Abschnitt 12 des Rendezvous.

Der letzte Weg zu einer Station, wenn ein Netz nur Web durchlaesst: TCP
443, vielleicht ueber einen Proxy, nichts auf UDP. Jedes Ende oeffnet
einen WebSocket zum Relay, und der Relay reicht die Datagramme der ICE-
Verbindung zwischen beiden Enden durch. Er sieht Chiffrat, Zeiten und
Groessen -- dasselbe wie coturn --, DTLS und SCTP bleiben Ende zu Ende.

**Wie der ICE-Agent den Relay benutzt.** Gar nicht bewusst: dieses Modul
oeffnet einen UDP-Port auf Loopback und gibt dem Agenten genau diese
Adresse als *entfernten Kandidaten* mit niedrigster Prioritaet. Was der
Agent dorthin schickt (STUN-Pruefungen, DTLS, SCTP), geht mit der
Stromkennung 1 durch den WebSocket; was von der Gegenseite kommt, wird
vom selben Port zurueck an den Agenten geschrieben. Der Agent glaubt, er
spreche mit der Gegenseite -- und alles, was er daraus macht, ist
Standard-ICE.

Die Gegenseite muss dasselbe tun (jedes Ende bekommt sein eigenes
Relay-Angebot, `relay.grant`, siehe `station.py`). Eine Webseite kann das
NICHT: ein Browser erlaubt seiner WebRTC-Verbindung keinen eigenen UDP-
Port, und eine Seite hat keinen Weg, dem ICE-Agenten einen Kandidaten
unterzuschieben, der nicht aus der Verbindung selbst stammt. Dieses Modul
ist darum die Stations- und die Python-Seite; fuer die Telefonseite siehe
README („Was noch fehlt").

Rahmen (12.3): jede Nachricht ist **binaer**, das erste Byte die Kennung.
0x01 Datagramm (Steuerstrom), 0x80 JOIN mit dem Token als ASCII, 0x81
READY (Fassung, Gegenseite da), 0x82 PEER, 0x83 END mit einem Code.
Hoechstens 1501 Byte je Nachricht. Wir nutzen nur den Strom 1: es gibt
bei uns eine ICE-Verbindung, nicht zwei.

Bricht der WebSocket ohne END, kommt das Ende binnen 30 s mit demselben
Token zurueck (12.4) -- die Loopback-Steckdose bleibt dabei dieselbe, der
Agent merkt nichts. Ein END sagt, was zu tun ist.
"""

from __future__ import annotations

import asyncio
import logging
import socket
from collections import deque
from typing import Any, Awaitable, Callable

log = logging.getLogger("fernzugriff.relais")

STROM = 0x01          # der Steuerstrom; wir haben nur eine ICE-Verbindung
JOIN, READY, PEER, END = 0x80, 0x81, 0x82, 0x83
MAX_DATAGRAMM = 1500

# Die Warteschlange vor dem WebSocket: 64 Nachrichten oder 24576 Byte, das
# Aelteste faellt zuerst (12.5) -- ein Rueckstau verzoegert sonst ohne Ende.
SCHLANGE_N = 64
SCHLANGE_BYTE = 24576

# So lange darf ein Bein weg sein, bevor der Relay die Sitzung beendet.
WIEDER_FRIST_S = 30.0

# END-Codes, nach denen dasselbe Angebot NICHT noch einmal taugt, und die,
# nach denen es nach einer kurzen Pause noch einmal geht (12.4).
ENDE_FERTIG = {"protocolError", "badToken", "expired", "ended", "peerGone", "idle",
               "replaced"}
ENDE_WIEDER = {"timeout", "full", "tooManyConnections", "tooManySessions",
               "shuttingDown"}

# Der Kandidat, mit dem der Agent den Relay als letzten Weg kennt. Prioritaet
# 1: jeder direkte und jeder TURN-Weg geht vor.
KANDIDAT_PRIORITAET = 1


def kandidat_text(adresse: tuple[str, int]) -> str:
    return f"candidate:relais 1 udp {KANDIDAT_PRIORITAET} {adresse[0]} {adresse[1]} typ host"


Verbinder = Callable[[], Awaitable[Any]]


async def verbinde_url(url: str) -> Any:
    """Der Normalfall: wss:// zum Relay hinter dem Webserver."""
    from websockets.asyncio.client import connect

    return await connect(url, max_size=MAX_DATAGRAMM + 1 + 16, open_timeout=10,
                         ping_interval=20, ping_timeout=20)


class _Steckdose(asyncio.DatagramProtocol):
    def __init__(self, bein: "Relaisbein") -> None:
        self.bein = bein
        self.transport: asyncio.DatagramTransport | None = None

    def connection_made(self, transport) -> None:  # noqa: ANN001
        self.transport = transport

    def datagram_received(self, daten: bytes, addr) -> None:  # noqa: ANN001
        self.bein._vom_agent(daten, addr)


class Relaisbein:
    """Ein Bein einer Relay-Sitzung: Loopback-Steckdose plus WebSocket."""

    def __init__(self, token: str, url: str = "", verbinder: Verbinder | None = None,
                 wiederfrist_s: float = WIEDER_FRIST_S) -> None:
        if not url and verbinder is None:
            raise ValueError("weder url noch verbinder")
        self._token = token
        self._verbinder = verbinder or (lambda: verbinde_url(url))
        self._wiederfrist = wiederfrist_s
        self._dose: _Steckdose | None = None
        self._adresse: tuple[str, int] | None = None     # die Steckdose selbst
        self._agent: tuple[str, int] | None = None       # woher der Agent sendet
        self._aus: deque[bytes] = deque()
        self._aus_byte = 0
        self._wecker = asyncio.Event()
        self._aufgabe: asyncio.Task | None = None
        self.gegenseite = False
        self.ende: str | None = None
        self.gezaehlt = {"hinaus": 0, "herein": 0, "verworfen": 0}
        self._offen_gewesen = asyncio.Event()

    @property
    def adresse(self) -> tuple[str, int]:
        assert self._adresse is not None, "erst starten"
        return self._adresse

    async def starte(self) -> tuple[str, int]:
        loop = asyncio.get_running_loop()
        transport, dose = await loop.create_datagram_endpoint(
            lambda: _Steckdose(self), local_addr=("127.0.0.1", 0),
            family=socket.AF_INET)
        self._dose = dose
        self._adresse = transport.get_extra_info("sockname")[:2]
        self._aufgabe = asyncio.ensure_future(self._laufe())
        return self._adresse

    async def warte_auf_bereit(self, frist: float = 10.0) -> bool:
        try:
            await asyncio.wait_for(self._offen_gewesen.wait(), frist)
            return True
        except asyncio.TimeoutError:
            return False

    # -- vom Agenten zum Relay -------------------------------------------

    def _vom_agent(self, daten: bytes, addr) -> None:  # noqa: ANN001
        # Die erste Pruefung des Agenten verraet, an welche Adresse
        # zurueckzuschreiben ist; ohne sie ist nichts zu tun.
        self._agent = addr
        if not daten or len(daten) > MAX_DATAGRAMM:
            # Nie senden, was der Relay mit 1009 beantworten wuerde, und nie
            # etwas Leeres (12.3).
            self.gezaehlt["verworfen"] += 1
            return
        nachricht = bytes([STROM]) + daten
        self._aus.append(nachricht)
        self._aus_byte += len(nachricht)
        while len(self._aus) > SCHLANGE_N or self._aus_byte > SCHLANGE_BYTE:
            alt = self._aus.popleft()
            self._aus_byte -= len(alt)
            self.gezaehlt["verworfen"] += 1
        self._wecker.set()

    # -- der WebSocket ---------------------------------------------------

    async def _laufe(self) -> None:
        weg_seit: float | None = None
        pause = 0.2
        while self.ende is None:
            try:
                ws = await self._verbinder()
            except asyncio.CancelledError:
                raise
            except Exception as e:
                log.info("Relay nicht erreichbar: %s", e)
                ws = None
            if ws is not None:
                weg_seit = None
                pause = 0.2
                try:
                    await self._ein_bein(ws)
                except asyncio.CancelledError:
                    raise
                except Exception as e:
                    log.info("Relay-Verbindung ging weg: %s", e)
                finally:
                    try:
                        await ws.close()
                    except Exception:
                        pass
                self.gegenseite = False
            if self.ende is not None:
                break
            loop_zeit = asyncio.get_running_loop().time()
            weg_seit = weg_seit if weg_seit is not None else loop_zeit
            if loop_zeit - weg_seit > self._wiederfrist:
                self.ende = "wegGeblieben"
                break
            await asyncio.sleep(pause)
            pause = min(pause * 2, 2.0)
        log.info("Relay-Bein zu (%s)", self.ende)
        self._offen_gewesen.set()    # damit niemand ewig wartet
        self._schliesse_dose()

    async def _ein_bein(self, ws) -> None:  # noqa: ANN001
        await ws.send(bytes([JOIN]) + self._token.encode("ascii"))
        leser = asyncio.ensure_future(self._lies(ws))
        schreiber = asyncio.ensure_future(self._schreibe(ws))
        try:
            await asyncio.wait({leser, schreiber}, return_when=asyncio.FIRST_COMPLETED)
        finally:
            for a in (leser, schreiber):
                a.cancel()
            for a in (leser, schreiber):
                try:
                    await a
                except (asyncio.CancelledError, Exception):
                    pass

    async def _schreibe(self, ws) -> None:  # noqa: ANN001
        while True:
            while self._aus:
                nachricht = self._aus.popleft()
                self._aus_byte -= len(nachricht)
                await ws.send(nachricht)
                self.gezaehlt["hinaus"] += 1
            self._wecker.clear()
            if not self._aus:
                await self._wecker.wait()

    async def _lies(self, ws) -> None:  # noqa: ANN001
        async for nachricht in ws:
            if isinstance(nachricht, str) or not nachricht:
                continue
            kennung, nutzlast = nachricht[0], nachricht[1:]
            if kennung == STROM:
                self._zum_agenten(nutzlast)
            elif kennung == READY:
                self.gegenseite = len(nutzlast) >= 2 and nutzlast[1] == 1
                self._offen_gewesen.set()
            elif kennung == PEER:
                self.gegenseite = len(nutzlast) >= 1 and nutzlast[0] == 1
            elif kennung == END:
                code = nutzlast[:64].decode("ascii", "replace")
                self._nimm_ende(code)
                return
            # alles andere: unbekannte Relay-Nachricht, uebergehen (12.3)

    def _nimm_ende(self, code: str) -> None:
        if code in ENDE_WIEDER:
            log.info("Relay sagt %s -- gleich noch einmal", code)
            return          # die Schleife verbindet nach kurzer Pause neu
        # Alles andere -- auch ein unbekannter Code -- heisst: vorbei.
        log.info("Relay sagt %s -- dieses Angebot ist verbraucht", code)
        self.ende = code

    def _zum_agenten(self, daten: bytes) -> None:
        if self._agent is None or self._dose is None or self._dose.transport is None:
            return
        self.gezaehlt["herein"] += 1
        self._dose.transport.sendto(daten, self._agent)

    # -- Ende ------------------------------------------------------------

    def _schliesse_dose(self) -> None:
        if self._dose is not None and self._dose.transport is not None:
            self._dose.transport.close()

    async def schliesse(self) -> None:
        if self.ende is None:
            self.ende = "geschlossen"
        if self._aufgabe is not None:
            self._aufgabe.cancel()
            try:
                await self._aufgabe
            except (asyncio.CancelledError, Exception):
                pass
            self._aufgabe = None
        self._schliesse_dose()
