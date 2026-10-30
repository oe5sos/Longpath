"""Ein Telefon aufnehmen -- mit einem Tipp, im eigenen Netz.

Bis hierher musste der Geraeteschluessel von Hand uebergeben werden: 122
Zeichen abtippen. Das ist niemandem zuzumuten, und ohne einen gangbaren
Weg ins Geraeteverzeichnis ist der ganze Fernzugriff unbenutzbar.

Das hier ist NereusSDRs „ein Tipp" (Station-Link 3.6, `mode: "lan"`),
auf unsere Verhaeltnisse gebracht: solange ein **Koppelfenster** offen
ist, nimmt die Station einen Schluessel entgegen, der aus einem ihrer
eigenen Netze kommt. Danach ist das Fenster zu.

**Was das NICHT ist: die Kopplung per Code.** Die laeuft bei NereusSDR
ueber SPAKE2+EE und traegt auch durch ein fremdes Netz. Dafuer gibt es
im Browser derzeit nichts Geprueftes (2026-10-28 nachgesehen: eine
WASM-Portierung und zwei ungepruefte JS-Fassungen, eine davon
ausdruecklich nicht laufzeitkonstant), und ein Kopplungsverfahren ist
die letzte Stelle, an der man etwas Ungeprueftes einbaut. Bis das
sauber geht, koppelt man einmal zu Hause -- danach traegt der
Fernzugriff von ueberall.

**Die drei Schranken**, und warum jede einzelne noetig ist:

1. **Das Fenster.** Ohne Fenster nimmt die Station nichts an. Es oeffnet
   nur, wer am Rechner sitzt (`fuehren.py --koppeln-offen`), und es
   schliesst nach der ersten Aufnahme oder nach seiner Frist.
2. **Das eigene Netz.** Angenommen wird nur, was von einer Adresse
   kommt, die in einem Netz einer laufenden Schnittstelle dieses
   Rechners liegt -- oder von Loopback. Ein Geraet aus dem Internet
   kommt hier nicht durch, auch nicht bei offenem Fenster.
3. **Ein Schluessel je Fenster.** Danach ist zu. Wer zweimal koppeln
   will, oeffnet zweimal.

NereusSDRs eigener Hinweis gilt auch hier: „ein Tipp" vertraut JEDEM
Netz, in dem der Rechner steht, ein VPN eingeschlossen. Wer das nicht
will, laesst das Fenster zu und traegt den Schluessel von Hand ein.
"""

from __future__ import annotations

import ipaddress
import json
import logging
import socket
import time
from dataclasses import dataclass

log = logging.getLogger("fernzugriff.koppeln")

# Wie lange ein Fenster hoechstens offen bleibt.
FENSTER_S = 300.0


@dataclass
class Fenster:
    """Ein offenes Koppelfenster."""

    bis: float
    genommen: bool = False

    @property
    def offen(self) -> bool:
        return not self.genommen and time.time() < self.bis

    @property
    def rest_s(self) -> float:
        return max(0.0, self.bis - time.time())


def eigene_netze() -> list[ipaddress.IPv4Network | ipaddress.IPv6Network]:
    """Die Netze der laufenden Schnittstellen dieses Rechners.

    Ueber `ifaddr` -- das liegt wegen aiortc ohnehin im Baum und bringt
    die Praefixlaenge je Adresse mit. Der naheliegende Weg ueber
    `socket.getaddrinfo(socket.gethostname())` taugt hier NICHT: auf
    diesem Mac wirft er `nodename nor servname provided`, weil der
    Rechnername nicht aufloest -- und das ausgerechnet nach einem
    Netzwechsel, also genau dann, wenn man koppeln will.
    """
    import ifaddr

    netze: list = []
    for karte in ifaddr.get_adapters():
        for adr in karte.ips:
            roh = adr.ip[0] if isinstance(adr.ip, tuple) else adr.ip
            try:
                ip = ipaddress.ip_address(roh)
            except ValueError:
                continue
            if ip.is_loopback or ip.is_link_local:
                continue
            try:
                netze.append(ipaddress.ip_network(
                    f"{ip}/{adr.network_prefix}", strict=False))
            except ValueError:
                continue
    return netze


def im_eigenen_netz(adresse: str) -> bool:
    """Kommt diese Adresse aus einem Netz, in dem dieser Rechner steht?"""
    try:
        ip = ipaddress.ip_address(adresse)
    except ValueError:
        return False
    if ip.is_loopback:
        return True
    return any(ip in netz for netz in eigene_netze())


class Koppelstelle:
    """Nimmt Schluessel entgegen, solange ein Fenster offen ist."""

    def __init__(self, geraete, fenster_s: float = FENSTER_S) -> None:
        self.geraete = geraete
        self.fenster_s = fenster_s
        self._fenster: Fenster | None = None

    def oeffne(self) -> float:
        self._fenster = Fenster(bis=time.time() + self.fenster_s)
        log.info("Koppelfenster offen fuer %.0f s", self.fenster_s)
        return self.fenster_s

    def schliesse(self) -> None:
        self._fenster = None

    @property
    def offen(self) -> bool:
        return self._fenster is not None and self._fenster.offen

    @property
    def rest_s(self) -> float:
        return self._fenster.rest_s if self._fenster else 0.0

    def nimm(self, schluessel: str, name: str, art: str, absender: str) -> dict:
        """Einen Schluessel aufnehmen. Gibt die Antwort fuer die Seite zurueck."""
        if not self.offen:
            # Dieselbe Antwort fuer „kein Fenster" und „schon genommen":
            # wer von aussen klopft, soll nicht lernen, wann jemand
            # koppelt.
            return {"ok": False, "grund": "Es ist gerade kein Koppelfenster offen."}
        if not im_eigenen_netz(absender):
            log.warning("Koppelversuch von %s -- nicht aus dem eigenen Netz", absender)
            return {"ok": False, "grund": "Dieses Geraet ist nicht im Netz der Station."}
        try:
            geraet = self.geraete.koppele(schluessel.strip(), name or "Telefon", art or "phone")
        except ValueError as e:
            return {"ok": False, "grund": f"Der Schluessel passt nicht: {e}"}
        self._fenster.genommen = True
        log.info("%s aufgenommen (%s), Fenster zu", geraet.name, geraet.ident[:8])
        return {"ok": True, "ident": geraet.ident, "name": geraet.name}


def antwort(stelle: Koppelstelle, rumpf: bytes, absender: str) -> tuple[int, bytes]:
    """Eine Anfrage der Seite beantworten (JSON rein, JSON raus)."""
    try:
        m = json.loads(rumpf or b"{}")
    except Exception:
        return 400, json.dumps({"ok": False, "grund": "unlesbar"}).encode()
    if not isinstance(m, dict) or not m.get("schluessel"):
        # Ohne Schluessel ist es eine Frage nach dem Zustand.
        return 200, json.dumps({
            "offen": stelle.offen,
            "rest": round(stelle.rest_s),
        }).encode()
    e = stelle.nimm(str(m["schluessel"]), str(m.get("name", "")),
                    str(m.get("art", "")), absender)
    return (200 if e.get("ok") else 403), json.dumps(e).encode()
