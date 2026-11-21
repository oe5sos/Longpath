#!/usr/bin/env python3
"""Haelt ein nachgelieferter Rahmen die folgenden Tonbloecke auf?

Der Verdacht vom 2026-10-28: der Datenkanal fuer TCI ist geordnet und
verlaesslich (SCTP), also wartet jeder Block hinter einem verlorenen, bis
dessen Wiederholung da ist. Auf Loopback gibt es keinen Verlust -- dort
konnte der Verdacht nicht belegt werden, und das Stocken vom 28.10. war
der Schub der Pumpe (siehe `pruefe-tontakt.py`).

Dieser Pruefstand stellt die Strecke nach, die ein Mobilfunknetz ist:
alles laeuft durch den WebSocket-Relay (`relais.py`), und dort geht ein
Teil der Datagramme verloren und jedes braucht seine Zeit. Gemessen werden
die Abstaende der Opus-Bloecke am Telefon.

    python3 handfunke/fernzugriff/pruefe-tonkanal.py
    python3 handfunke/fernzugriff/pruefe-tonkanal.py --verlust 0.10 --dauer 30

Gefahren werden **beide** Faelle in demselben Lauf, der geordnete Kanal
zuerst: `LONGPATH_TONKANAL=0` (alles auf einem Kanal) und `=1` (die
Opus-Bloecke auf einem eigenen, ungeordneten Kanal ohne Wiederholung).
Das Ergebnis ist ein Vergleich, keine Schwelle -- ein guter Wert ohne
Gegenprobe belegt nichts.

Still: es wird kein Ton abgespielt, nur die Ankunftszeiten zaehlen.
"""

from __future__ import annotations

import argparse
import asyncio
import contextlib
import importlib.util
import os
import statistics
import sys
import tempfile
import time
from pathlib import Path

HIER = Path(__file__).resolve().parent
sys.path.insert(0, str(HIER))

import relais  # noqa: E402
from geraet import Geraeteseite  # noqa: E402
from geraete import Geraeteliste  # noqa: E402
from sitzung import Ziel  # noqa: E402
from station import Station  # noqa: E402
from ton import lies_kopf  # noqa: E402

_s = importlib.util.spec_from_file_location("pruefe_relay", HIER / "pruefe-relay.py")
rl = importlib.util.module_from_spec(_s)
_s.loader.exec_module(rl)

schlecht = 0


def fehler(z: str) -> None:
    global schlecht
    schlecht += 1
    print("  FEHLER:", z)


async def miss(url, tmp, tci_port, verbinder, name, dauer, tonkanal) -> dict:
    os.environ["LONGPATH_TONKANAL"] = "1" if tonkanal else "0"
    # Die Station liest den Schalter beim Anlegen der Sitzung.
    st, liste, halten, ok = await rl.mit_station(
        url, tmp, tci_port, name, nur_relais=True,
        relais_verbinder=verbinder, relais_verzoegerung_s=0.2)
    try:
        if not ok:
            return {"pakete": 0}
        g = Geraeteseite(url, tmp / f"tel-{name}.pem", st.lage.ident, kann=["opus"],
                         nur_relais=True, relais_verbinder=verbinder,
                         relais_verzoegerung_s=0.2)
        liste.koppele(g.schluessel_text, "Pruefstand", "phone")
        abstaende: list[float] = []
        laufzeiten: list[float] = []
        letzte = [0.0]
        sendelog: list = []
        naechster = [0]
        sitzung = next(iter(st._sitzungen.values()), None)

        def ankunft(m):
            if not isinstance(m, (bytes, bytearray)) or lies_kopf(bytes(m)) is None:
                return
            jetzt = asyncio.get_running_loop().time()
            if letzte[0]:
                abstaende.append((jetzt - letzte[0]) * 1000.0)
            letzte[0] = jetzt
            # Laufzeit: den Block im Sendeprotokoll der Station suchen,
            # vorwaerts ab dem zuletzt gefundenen. Ein verlorener Block
            # mit gleichem Inhalt kann die Zuordnung um 20 ms verschieben;
            # das ist weniger als jede Grenze, um die es hier geht.
            for i in range(naechster[0], len(sendelog)):
                if sendelog[i][1] == bytes(m):
                    laufzeiten.append((jetzt - sendelog[i][0]) * 1000.0)
                    naechster[0] = i + 1
                    break

        g.beim_empfang = ankunft
        # Das Sendeprotokoll haengt an der Sitzung; sie entsteht beim
        # Anklopfen, also erst NACH dem Verbinden.
        try:
            await g.verbinde(frist=40.0)
        except Exception as e:
            fehler(f"{name}: keine Verbindung ({type(e).__name__} {e})")
            return {"pakete": 0}
        sitzung = next(iter(st._sitzungen.values()))
        sitzung.sendelog = sendelog
        g.sende("start;")
        g.sende("audio_start:0;")
        await asyncio.sleep(dauer)
        kanaele = st._sitzungen and next(iter(st._sitzungen.values())).ton_ueber_eigenen_kanal
        await g.schliesse()
    finally:
        await rl.beende(st, halten)
    if not abstaende:
        return {"pakete": 0}
    s = sorted(abstaende)
    lz = sorted(laufzeiten) or [0.0]
    return {
        "lz_median": lz[len(lz) // 2],
        "lz_p95": lz[int(len(lz) * 0.95)],
        "lz_p99": lz[int(len(lz) * 0.99)],
        "lz_max": lz[-1],
        "lz_ueber_250": sum(1 for x in lz if x > 250.0),
        "im_schub": sum(1 for x in s if x < 8.0),
        "pakete": len(s) + 1,
        "median": s[len(s) // 2],
        "p99": s[int(len(s) * 0.99)],
        "max": s[-1],
        "ueber_150": sum(1 for x in s if x > 150.0),
        "ueber_500": sum(1 for x in s if x > 500.0),
        "streuung": statistics.pstdev(s),
        "eigener": bool(kanaele),
    }


async def lauf(a) -> None:
    rv_port, tci_port = rl.freier_port(), rl.freier_port()
    url = f"ws://127.0.0.1:{rv_port}/"
    with tempfile.TemporaryDirectory() as tmp, \
            tempfile.TemporaryDirectory(prefix="nr", dir="/tmp") as sockdir:
        tmp = Path(tmp)
        sockel = os.path.join(sockdir, "relay.sock")
        geheim = tmp / "relay-secret"
        import secrets
        geheim.write_text(secrets.token_hex(32))
        geheim.chmod(0o600)
        (tmp / "rv.conf").write_text(
            f"[rendezvous]\nlisten = 127.0.0.1:{rv_port}\nstun_urls =\nlog_level = warning\n"
            f"relay_url = wss://rv.invalid/v1/relay\nrelay_secret_file = {geheim}\n")
        (tmp / "relay.conf").write_text(
            f"[relay]\nsocket = {sockel}\nsocket_mode = 0600\nsocket_group =\n"
            f"relay_secret_file = {geheim}\nlog_level = warning\n")
        verbinder = rl.verbinder_fuer(sockel)
        erg = {}
        with rl.laeuft([sys.executable, "-m", "nereus_rendezvous", "--config", str(tmp / "rv.conf")],
                       rl.DIENST, lambda: rl.port_offen(rv_port), "der Dienst"), \
             rl.laeuft([sys.executable, "-m", "nereus_relay", "--config", str(tmp / "relay.conf")],
                       rl.DIENST, lambda: os.path.exists(sockel), "der Relay"), \
             rl.laeuft([sys.executable, str(rl.ATTRAPPE), "--port", str(tci_port)],
                       rl.WURZEL, lambda: rl.port_offen(tci_port), "die Attrappe"):
            for lauf_nr in range(a.laeufe):
                for verlust in (0.0, a.verlust):
                    relais.STOERUNG["verlust"] = verlust
                    relais.STOERUNG["verzoegerung_s"] = a.verzoegerung
                    for tonkanal in (False, True):
                        name = f"{'k' if tonkanal else 'g'}{int(verlust*100)}_{lauf_nr}"
                        print(f"  {'eigener Tonkanal' if tonkanal else 'ein Kanal':17}"
                              f" Verlust {verlust*100:.0f} % …", flush=True)
                        erg[(verlust, tonkanal, lauf_nr)] = await miss(
                            url, tmp, tci_port, verbinder, name, a.dauer, tonkanal)
                        await asyncio.sleep(0.5)

    print()
    print("Laufzeit = vom Absenden an der Station bis zur Ankunft am Telefon")
    print(f"{'':36} {'Pakete':>6} {'LZ med':>7} {'LZ p95':>7} {'LZ p99':>7} {'LZ max':>7} {'>250ms':>6} {'Schub':>5}")
    for (verlust, tonkanal, n), e in sorted(erg.items()):
        bez = f"Verlust {verlust*100:.0f} %, {'eigener Tonkanal' if tonkanal else 'ein Kanal'} #{n+1}"
        if not e.get("pakete"):
            print(f"{bez:36} kein Paket")
            continue
        print(f"{bez:36} {e['pakete']:>6} {e['lz_median']:>6.0f}m {e['lz_p95']:>6.0f}m "
              f"{e['lz_p99']:>6.0f}m {e['lz_max']:>6.0f}m {e['lz_ueber_250']:>6} {e['im_schub']:>5}")
    a.ergebnisse = erg


def mittel(erg, v, t, k):
    w = [e[k] for (vv, tt, _), e in erg.items() if vv == v and tt == t and e.get("pakete")]
    return sum(w) / len(w) if w else None


def auswerten(a) -> None:
    erg = a.ergebnisse
    v = a.verlust
    og, mg = mittel(erg, 0.0, False, "lz_p99"), mittel(erg, v, False, "lz_p99")
    ok, mk = mittel(erg, 0.0, True, "lz_p99"), mittel(erg, v, True, "lz_p99")
    if None in (og, mg, ok, mk):
        fehler("ohne Messwerte in allen vier Faellen keine Aussage")
        return
    print()
    print(f"  Laufzeit p99, Mittel der Laeufe: ein Kanal {og:.0f} -> {mg:.0f} ms, "
          f"eigener Tonkanal {ok:.0f} -> {mk:.0f} ms (ohne -> mit {v*100:.0f} % Verlust)")
    # 1. Der Verdacht: unter Verlust hebt der geordnete Kanal die Laufzeit.
    if mg - og >= 80.0:
        print(f"  -> VERDACHT BELEGT: der geordnete Kanal haelt unter Verlust Bloecke auf "
              f"(+{mg - og:.0f} ms im p99)")
    else:
        fehler(f"der Verdacht ist NICHT belegt: der geordnete Kanal wird unter Verlust nur "
               f"um {mg - og:.0f} ms langsamer (Schwelle 80 ms) -- dann ist kein eigener "
               f"Kanal noetig")
    # 2. Die Abhilfe.
    if not all(e.get("eigener") for (vv, tt, _), e in erg.items() if tt and e.get("pakete")):
        fehler("der Ton lief gar nicht ueber den eigenen Kanal")
    if mk >= mg:
        fehler("der eigene Tonkanal hat die Laufzeit unter Verlust nicht verringert")
    elif mk - ok > 40.0:
        fehler(f"der eigene Tonkanal wird unter Verlust trotzdem {mk - ok:.0f} ms langsamer")
    else:
        print(f"  -> der eigene Tonkanal haelt die Laufzeit (+{mk - ok:.0f} ms statt +{mg - og:.0f} ms)")


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--verlust", type=float, default=0.10)
    p.add_argument("--verzoegerung", type=float, default=0.025,
                   help="Sekunden je Richtung und Bein (RTT etwa das Vierfache)")
    p.add_argument("--dauer", type=float, default=30.0)
    p.add_argument("--laeufe", type=int, default=2)
    a = p.parse_args()
    asyncio.run(lauf(a))
    auswerten(a)
    print()
    print("ALLES GRUEN" if schlecht == 0 else f"{schlecht} Abweichungen")
    return 0 if schlecht == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
