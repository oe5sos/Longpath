#!/usr/bin/env python3
"""Den Opus-Weg messen: Ersparnis, Rundlauf und die Faelle, die durchgehen.

    python3 handfunke/fernzugriff/pruefe-ton.py

Geprueft wird, was traegt:

* aus Int16-Tonrahmen werden Opus-Rahmen, und der Strom wird dabei um
  mindestens das Dreifache kleiner,
* der eigene Kodierer gegen den eigenen Dekoder: was herauskommt, muss
  wieder als Ton erkennbar sein (Stoerabstand gemessen, nicht geraten),
* alles, was NICHT Ton in Int16 ist, geht unveraendert durch -- Spektrum,
  mu-law, I/Q. Ein Umkodierer, der zu viel anfasst, ist schlimmer als
  keiner.
"""

from __future__ import annotations

import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import av  # noqa: E402
import numpy as np  # noqa: E402

from ton import (  # noqa: E402
    OPUS_KOPF,
    TON_STROM,
    TYP_INT16,
    TYP_MULAW,
    Umkodierer,
    lies_kopf,
)

_KOPF = struct.Struct("<16I")
schlecht = 0


def fehler(z: str) -> None:
    global schlecht
    schlecht += 1
    print("  FEHLER:", z)


def gut(z: str) -> None:
    print("  ok:", z)


def tonrahmen(werte: np.ndarray, rate: int = 12000, typ: int = TYP_INT16,
              strom: int = TON_STROM) -> bytes:
    kopf = [0] * 16
    kopf[0] = 0
    kopf[1] = rate
    kopf[2] = typ
    kopf[5] = len(werte)
    kopf[6] = strom
    kopf[7] = 1
    return _KOPF.pack(*kopf) + werte.astype("<i2").tobytes()


def sprachaehnlich(n: int, rate: int = 12000) -> np.ndarray:
    """Etwas, das wie Stimme aussieht: Grundton, Obertoene, Huellkurve.

    Ein reiner Sinus waere zu gutmuetig -- Opus rechnet ihn fast
    verlustfrei, und die Messung saehe besser aus als die Wirklichkeit.
    """
    t = np.arange(n) / rate
    grund = 110.0
    s = np.zeros(n)
    for k, a in ((1, 1.0), (2, 0.5), (3, 0.3), (4, 0.18), (6, 0.1)):
        s += a * np.sin(2 * math.pi * grund * k * t)
    s *= 0.6 + 0.4 * np.sin(2 * math.pi * 3.0 * t)      # Silben
    s += 0.02 * np.random.default_rng(7).standard_normal(n)  # Bandrauschen
    s /= np.max(np.abs(s)) * 1.05
    return (s * 32767).astype(np.int16)


def pruefe_ersparnis() -> None:
    u = Umkodierer(bitrate=24000)
    rate = 12000
    quelle = sprachaehnlich(rate * 5, rate)     # fuenf Sekunden
    raus = []
    # In Stuecken, wie sie wirklich kommen: 1024 Werte je Rahmen.
    for i in range(0, len(quelle), 1024):
        r = u.rahmen(tonrahmen(quelle[i:i + 1024], rate))
        if r:
            raus.extend(r)
    if not raus:
        fehler("es kam kein einziger Opus-Rahmen heraus")
        return
    sek = len(quelle) / rate
    roh_kbs = u.roh_bytes / sek / 1000
    opus_kbs = u.opus_bytes / sek / 1000
    gut(f"{len(raus)} Rahmen, {roh_kbs:.1f} kB/s roh -> {opus_kbs:.1f} kB/s Opus "
        f"(Faktor {u.ersparnis:.1f})")
    if u.ersparnis < 3.0:
        fehler(f"nur Faktor {u.ersparnis:.1f} gespart, erwartet mindestens 3")
    # Gegen die 13,3 kB/s von mu-law, die heute fliessen.
    if opus_kbs > 4.5:
        fehler(f"{opus_kbs:.1f} kB/s ist zu viel -- mu-law liegt bei 13,3")


def pruefe_rundlauf() -> None:
    """Der eigene Kodierer gegen den eigenen Dekoder."""
    u = Umkodierer(bitrate=24000)
    rate = 12000
    quelle = sprachaehnlich(rate * 3, rate)
    pakete = []
    for i in range(0, len(quelle), 1024):
        for r in (u.rahmen(tonrahmen(quelle[i:i + 1024], rate)) or []):
            kopf = lies_kopf(r)
            if kopf is None:
                fehler("ein Opus-Rahmen traegt nicht unseren Kopf")
                return
            pakete.append(r[OPUS_KOPF:])

    d = av.CodecContext.create("libopus", "r")
    d.sample_rate = rate
    d.format = "s16"
    d.layout = "mono"
    zurueck = []
    aus_rate = None
    for p in pakete:
        for rahmen in d.decode(av.Packet(p)):
            if aus_rate is None:
                aus_rate = rahmen.sample_rate
            zurueck.append(rahmen.to_ndarray().reshape(-1))
    if not zurueck:
        fehler("der Dekoder gab nichts zurueck")
        return
    wieder = np.concatenate(zurueck).astype(np.float64)

    # **Opus gibt IMMER 48 kHz zurueck**, gleich welche Rate hineinging.
    # Das ist kein Fehler, sondern wie libopus arbeitet -- und fuer die
    # Seite sogar guenstig, weil ihr AudioContext ohnehin auf 48 kHz
    # laeuft und das Hochrechnen von 12 kHz damit entfaellt. Beim ersten
    # Anlauf hat es die Messung zerlegt: 144000 Werte gegen 36000
    # erwartete, Stoerabstand 0,0 dB fuer einen Ton, der in Ordnung war.
    if aus_rate != rate:
        schritt = aus_rate // rate
        if schritt * rate != aus_rate:
            fehler(f"Rueckgabe mit {aus_rate} Hz passt nicht zu {rate} Hz")
            return
        wieder = wieder[::schritt]
        gut(f"Dekoder gibt {aus_rate} Hz zurueck (auf {rate} gebracht)")

    orig = quelle.astype(np.float64)
    n = min(len(wieder), len(orig))
    orig, wieder = orig[:n], wieder[:n]

    # Ein Zeitbereichs-Stoerabstand sagt bei Opus wenig: es rechnet
    # wahrnehmungsorientiert und nicht wellenformtreu, die Phase darf
    # wandern. Gemessen wird darum, ob der INHALT heil ankommt --
    # Grundton und die Leistungsverteilung ueber die Baender.
    def spektrum(x):
        f = np.abs(np.fft.rfft(x * np.hanning(len(x))))
        return f / (np.sum(f) or 1.0)

    sa, sb = spektrum(orig), spektrum(wieder)
    hz = np.fft.rfftfreq(n, 1.0 / rate)
    grund_a = hz[np.argmax(sa[: n // 8])]
    grund_b = hz[np.argmax(sb[: n // 8])]
    # Baender bis 6 kHz, je eine Oktave.
    kanten = [0, 150, 300, 600, 1200, 2400, 4800, 6000]
    ab, bb = [], []
    for lo, hi in zip(kanten[:-1], kanten[1:]):
        m = (hz >= lo) & (hz < hi)
        ab.append(float(np.sum(sa[m])))
        bb.append(float(np.sum(sb[m])))
    abw = max(abs(x - y) for x, y in zip(ab, bb))
    gut(f"Grundton {grund_a:.0f} Hz -> {grund_b:.0f} Hz, "
        f"groesste Bandabweichung {abw * 100:.1f} Prozentpunkte")
    if abs(grund_a - grund_b) > 15:
        fehler(f"der Grundton wandert: {grund_a:.0f} -> {grund_b:.0f} Hz")
    if abw > 0.15:
        fehler(f"die Leistungsverteilung weicht um {abw * 100:.0f} Punkte ab")


def pruefe_durchlass() -> None:
    """Was nicht Ton in Int16 ist, muss unangetastet bleiben."""
    u = Umkodierer()
    werte = np.zeros(256, dtype=np.int16)
    faelle = [
        ("Spektrum (Strom 100)", tonrahmen(werte, 12000, 100, 100)),
        ("mu-law", tonrahmen(werte, 12000, TYP_MULAW)),
        ("I/Q (Strom 0)", tonrahmen(werte, 192000, TYP_INT16, 0)),
        ("Ton mit krummer Rate", tonrahmen(werte, 11025, TYP_INT16)),
        ("zu kurz", b"\x00" * 10),
    ]
    for name, roh in faelle:
        if u.rahmen(roh) is not None:
            fehler(f"{name} wurde angefasst, haette durchgehen muessen")
    gut(f"{len(faelle)} Faelle gehen unveraendert durch")


def main() -> int:
    pruefe_ersparnis()
    pruefe_rundlauf()
    pruefe_durchlass()
    print()
    print("ALLES GRUEN" if schlecht == 0 else f"{schlecht} Abweichungen")
    return 0 if schlecht == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
