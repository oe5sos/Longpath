#!/usr/bin/env python3
"""Misst, ob die Tonbloecke gleichmaessig ankommen -- mit und ohne Taktung.

    python3 handfunke/fernzugriff/pruefe-tontakt.py

**Still.** Es wird kein Ton abgespielt: gemessen werden die Ankunftszeiten
der Pakete am Datenkanal, nicht ihr Inhalt. Dafuer meldet die
Geraeteseite der Station "opus", ohne einen Dekoder zu haben. (Die
Messung im Browser am 2026-10-28 hat dagegen ueber die Lautsprecher
gepfiffen -- einmal reicht.)

Gefahren werden BEIDE Fassungen, die alte zuerst: ein guter Wert ohne
Gegenprobe belegt nichts.

**Was dieser Lauf NICHT zeigen kann.** Die Attrappe liefert den Ton
nicht in Echtzeit -- gemessen etwa 22 Bloecke je Sekunde, wo 50 noetig
waeren. Damit ist die Schlange der Tonpumpe fast immer leer, jeder Block
geht sofort hinaus, und die Ausgabe folgt dem Eingangstakt statt dem
eigenen. Der Lauf belegt also, dass der Opus-Weg ueberhaupt traegt, und
nicht, dass die Taktung glaettet. Dafuer braucht es Longpath am echten
Geraet.
"""

from __future__ import annotations

import asyncio
import contextlib
import os
import socket
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from geraet import Geraeteseite  # noqa: E402
from geraete import Geraeteliste  # noqa: E402
from ton import lies_kopf  # noqa: E402

WURZEL = Path(__file__).resolve().parents[2]
DIENST = WURZEL / "rendezvous" / "server"
ATTRAPPE = WURZEL / "handfunke" / "attrappe.py"
MESSDAUER_S = 20.0
schlecht = 0


def fehler(z: str) -> None:
    global schlecht
    schlecht += 1
    print("  FEHLER:", z)


def freier_port() -> int:
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def warte_auf_port(port: int, p: subprocess.Popen, was: str) -> None:
    for _ in range(300):
        with socket.socket() as s:
            if s.connect_ex(("127.0.0.1", port)) == 0:
                return
        if p.poll() is not None:
            raus = p.stdout.read().decode() if p.stdout else ""
            raise RuntimeError(f"{was} ist gestorben:\n{raus}")
        time.sleep(0.05)
    raise RuntimeError(f"{was} nimmt nicht an")


@contextlib.contextmanager
def laeuft(befehl, cwd, port, was, umgebung=None):
    u = dict(os.environ)
    u.update(umgebung or {})
    p = subprocess.Popen(befehl, cwd=cwd, env=u,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        warte_auf_port(port, p, was)
        yield p
    finally:
        p.terminate()
        with contextlib.suppress(subprocess.TimeoutExpired):
            p.wait(timeout=5)


async def miss(url: str, station_id: str, schluessel: Path) -> dict:
    """Eine Verbindung aufbauen und die Abstaende der Tonpakete messen."""
    g = Geraeteseite(url, schluessel, station_id, kann=["opus"])
    abstaende: list[float] = []
    letzte = [0.0]

    def ankunft(nachricht):
        if not isinstance(nachricht, (bytes, bytearray)):
            return
        if lies_kopf(bytes(nachricht)) is None:
            return
        jetzt = time.perf_counter()
        if letzte[0]:
            abstaende.append((jetzt - letzte[0]) * 1000.0)
        letzte[0] = jetzt

    g.beim_empfang = ankunft
    await g.verbinde(frist=25.0)
    g.sende("start;")
    g.sende("audio_start:0;")
    await asyncio.sleep(MESSDAUER_S)
    await g.schliesse()

    if not abstaende:
        return {"pakete": 0}
    s = sorted(abstaende)
    return {
        "pakete": len(s) + 1,
        "median": s[len(s) // 2],
        "p90": s[int(len(s) * 0.9)],
        "max": s[-1],
        "streuung": statistics.pstdev(s),
        "im_schub": sum(1 for x in s if x < 2.0),
        "luecken": sum(1 for x in s if x > 50.0),
    }


async def lauf() -> None:
    rv_port, tci_port = freier_port(), freier_port()
    url = f"ws://127.0.0.1:{rv_port}/"
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        with tempfile.NamedTemporaryFile("w", suffix=".conf", delete=False) as f:
            f.write(f"[rendezvous]\nlisten = 127.0.0.1:{rv_port}\nstun_urls =\nlog_level = warning\n")
            conf = f.name

        ergebnisse = {}
        with laeuft([sys.executable, "-m", "nereus_rendezvous", "--config", conf],
                    DIENST, rv_port, "der Dienst"), \
             laeuft([sys.executable, str(ATTRAPPE), "--port", str(tci_port)],
                    WURZEL, tci_port, "die Attrappe"):

            liste = Geraeteliste(tmp / "geraete.json")
            telefon_schluessel = tmp / "telefon.pem"
            vorweg = Geraeteseite(url, telefon_schluessel, "a" * 26)
            liste.koppele(vorweg.schluessel_text, "Pruefstand")

            # Die alte Fassung ZUERST.
            for name, takt in (("ohne Taktung (alt)", "0"), ("mit Taktung (neu)", "1")):
                proc = subprocess.Popen(
                    [sys.executable, str(WURZEL / "handfunke" / "fernzugriff" / "fuehren.py"),
                     "--dienst", url, "--longpath", f"ws://127.0.0.1:{tci_port}",
                     "--ort", str(tmp), "--leise"],
                    env={**os.environ, "LONGPATH_TONTAKT": takt},
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                try:
                    # Auf die Anmeldung warten, nicht auf die Uhr.
                    ident = None
                    for _ in range(200):
                        zeile = proc.stdout.readline().decode()
                        if "Stations-Id:" in zeile:
                            ident = zeile.split(":")[1].strip()
                        if ident and "angemeldet" in zeile and "nicht" not in zeile:
                            break
                        if proc.poll() is not None:
                            raise RuntimeError("die Station ist gestorben")
                    if not ident:
                        raise RuntimeError("die Station hat ihre Id nicht genannt")
                    print(f"  {name} …", flush=True)
                    ergebnisse[name] = await miss(url, ident, telefon_schluessel)
                finally:
                    proc.terminate()
                    with contextlib.suppress(subprocess.TimeoutExpired):
                        proc.wait(timeout=5)
                    await asyncio.sleep(1.0)

        Path(conf).unlink(missing_ok=True)

    print()
    print(f"{'':22} {'Pakete':>7} {'Median':>8} {'p90':>8} {'max':>8} "
          f"{'Streuung':>9} {'im Schub':>9} {'Luecken':>8}")
    for name, e in ergebnisse.items():
        if not e.get("pakete"):
            fehler(f"{name}: kein einziges Tonpaket")
            continue
        print(f"{name:22} {e['pakete']:>7} {e['median']:>7.1f}ms {e['p90']:>7.1f}ms "
              f"{e['max']:>7.1f}ms {e['streuung']:>8.1f}ms {e['im_schub']:>9} {e['luecken']:>8}")

    alt = ergebnisse.get("ohne Taktung (alt)", {})
    neu = ergebnisse.get("mit Taktung (neu)", {})
    if alt.get("pakete") and neu.get("pakete"):
        print()
        if neu["im_schub"] >= alt["im_schub"] and alt["im_schub"] > 0:
            fehler(f"die Taktung hat nichts gebracht: {alt['im_schub']} -> "
                   f"{neu['im_schub']} Pakete im Schub")
        else:
            print(f"  Pakete im Schub: {alt['im_schub']} -> {neu['im_schub']}")
        if neu["streuung"] > alt["streuung"] and alt["streuung"] > 1.0:
            fehler(f"die Streuung ist groesser geworden: {alt['streuung']:.1f} -> "
                   f"{neu['streuung']:.1f} ms")
        else:
            print(f"  Streuung: {alt['streuung']:.1f} -> {neu['streuung']:.1f} ms")


def main() -> int:
    asyncio.run(lauf())
    print()
    print("ALLES GRUEN" if schlecht == 0 else f"{schlecht} Abweichungen")
    return 0 if schlecht == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
