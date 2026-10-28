"""Den Tonstrom fuer die Mobilfunkstrecke kleinrechnen.

Der Ton ist mit Abstand der teuerste Teil: bei 12 kHz mono Int16 sind das
24,9 kB/s von 29,5 insgesamt. mu-law (TciSampleType 101, Longpath-eigen)
halbiert das auf 13,3 kB/s. Ueber Opus bei 24 kbit/s sind es **3 kB/s** --
elf Megabyte in der Stunde statt hundertsechs.

**Warum das hier steht und nicht in Longpath.** Im TCI-Server waere es
auch zu haben (libopus haengt wegen RADE ohnehin an der
Verknuepfungszeile), aber dort steht es im Weg: `encodeSamples` rechnet
mit einer festen Byte-Zahl je Abtastung, und Opus ist ein Rahmenkodierer
mit veraenderlicher Laenge und eigenem Zustand je Verbindung. Vor allem
aber ist TCI ein Protokoll, das fremde Programme sprechen. Hier dagegen
sind beide Enden unsere eigenen, die Strecke ist genau die teure, und im
eigenen WLAN aendert sich nichts: dort bleibt mu-law, und 13 kB/s ueber
WLAN tun niemandem weh.

**Die Voraussetzung, die lange gefehlt hat.** WebCodecs' `AudioDecoder`
ist `[SecureContext]`. Auf `http://172.30.30.x` gibt es ihn nicht -- am
2026-09-30 und 2026-10-04 am Telefon gemessen, und genau daran ist Opus
damals gescheitert. Ueber den Fernzugriff kommt die Seite von einem
`https`-Ort, dort ist der Kontext sicher. Wer trotzdem keinen Dekoder
hat, bekommt weiter mu-law: der Weg hier wird nur eingeschaltet, wenn
das Geraet beim Anmelden sagt, dass es Opus kann.
"""

from __future__ import annotations

import logging
import struct
from fractions import Fraction

import av
import numpy as np

log = logging.getLogger("fernzugriff.ton")

# Der TCI-Binaerkopf, wie ihn TciBinaryFrame.h festlegt.
KOPF = 64
_KOPF = struct.Struct("<16I")

TON_STROM = 1          # TciStreamType::RxAudioStream
TYP_INT16 = 0
TYP_MULAW = 101
# Unsere eigene Kennzahl. Sie verlaesst diese Strecke nie: Longpath kennt
# sie nicht und bekommt sie nie zu sehen, und ein fremder Client ist hier
# nicht angeschlossen.
TYP_OPUS = 102

# ── Warum der Opus-Rahmen einen EIGENEN, kurzen Kopf hat ──────────────────
#
# Der TCI-Kopf ist 64 Byte lang. Das faellt bei 1024 Werten Int16 (2 KiB)
# nicht auf, bei einem Opus-Block von 20 ms aber sehr: gemessen 3,5 kB/s
# Nutzlast gegen 3,2 kB/s Kopf -- der Kopf haette die Ersparnis fast
# halbiert (6,7 statt 3,9 kB/s, erster Lauf am 2026-10-28).
#
# Auf dieser Strecke sind beide Enden unsere eigenen, also traegt der
# Opus-Rahmen nur, was die Gegenseite wirklich braucht: eine Kennung,
# damit er von einem TCI-Rahmen zu unterscheiden ist, die Abtastrate,
# die Zahl der Werte im Block und den Empfaenger.
#
#   Byte 0..3  "LPO1"  (Longpath Opus, Fassung 1)
#   Byte 4..5  Abtastrate / 50   (12000 -> 240; ganzzahlig bis 3,2 MHz)
#   Byte 6..7  Werte im Block
#   Byte 8     Empfaenger
#   Byte 9     Kanaele
#   ab 10      das Opus-Paket
OPUS_KOPF = 10
OPUS_MARKE = b"LPO1"
_OPUS = struct.Struct("<4sHHBB")

# 20 ms bei 12 kHz. Opus kann 2,5 bis 60 ms; 20 ist der uebliche
# Kompromiss aus Verzoegerung und Mehraufwand je Rahmen.
BLOCK_MS = 20


class Umkodierer:
    """Macht aus Int16-Tonrahmen Opus-Rahmen -- einer je Verbindung.

    Der Kodierer traegt Zustand (die Vorhersage des vorigen Blocks), darum
    gehoert er der Sitzung und nicht dem Modul.
    """

    def __init__(self, bitrate: int = 24000) -> None:
        self.bitrate = bitrate
        self._kodierer = None
        self._rate = 0
        self._rest = np.zeros(0, dtype=np.int16)
        self.roh_bytes = 0      # was hereinkam
        self.opus_bytes = 0     # was hinausging

    # -- Aufbau ----------------------------------------------------------

    def _richte_ein(self, rate: int) -> None:
        if self._kodierer is not None and self._rate == rate:
            return
        k = av.CodecContext.create("libopus", "w")
        k.sample_rate = rate
        k.format = "s16"
        k.layout = "mono"
        k.bit_rate = self.bitrate
        # Opus kennt nur 8, 12, 16, 24 und 48 kHz. Longpath liefert 12 --
        # eine andere Rate waere ein Fall fuer einen Umsetzer, und bis
        # dahin bleibt es bei mu-law.
        k.open()
        self._kodierer = k
        self._rate = rate
        self._rest = np.zeros(0, dtype=np.int16)
        log.info("Opus an: %d Hz, %d bit/s, %d Werte je Block",
                 rate, self.bitrate, k.frame_size)

    def kann(self, rate: int) -> bool:
        return rate in (8000, 12000, 16000, 24000, 48000)

    # -- Arbeit ----------------------------------------------------------

    def rahmen(self, roh: bytes) -> list[bytes] | None:
        """Einen TCI-Rahmen umkodieren.

        Gibt die neuen Rahmen zurueck, oder None, wenn dieser Rahmen nicht
        hierher gehoert (dann reicht ihn der Aufrufer unveraendert durch).
        Ein Tonrahmen kann null, einen oder mehrere Opus-Rahmen ergeben:
        Opus arbeitet in festen Bloecken, die Eingangsrahmen nicht.
        """
        if len(roh) < KOPF:
            return None
        kopf = _KOPF.unpack_from(roh, 0)
        empfaenger, rate, typ, laenge, strom = kopf[0], kopf[1], kopf[2], kopf[5], kopf[6]
        if strom != TON_STROM:
            return None
        if typ != TYP_INT16:
            # mu-law oder Float32 zuerst nach Int16 zurueckzurechnen waere
            # doppelt verlustbehaftet. Die App fordert darum int16 an,
            # sobald sie Opus kann.
            return None
        if not self.kann(rate):
            return None

        self._richte_ein(rate)
        n = min(laenge, (len(roh) - KOPF) // 2)
        if n <= 0:
            return None
        werte = np.frombuffer(roh, dtype="<i2", count=n, offset=KOPF)
        self.roh_bytes += len(roh)

        self._rest = np.concatenate((self._rest, werte))
        block = self._kodierer.frame_size
        aus: list[bytes] = []
        while len(self._rest) >= block:
            stueck = self._rest[:block]
            self._rest = self._rest[block:]
            rahmen = av.AudioFrame.from_ndarray(
                stueck.reshape(1, -1), format="s16", layout="mono")
            rahmen.sample_rate = self._rate
            rahmen.time_base = Fraction(1, self._rate)
            for paket in self._kodierer.encode(rahmen):
                nutz = bytes(paket)
                aus.append(_baue(empfaenger, self._rate, block, nutz))
                self.opus_bytes += OPUS_KOPF + len(nutz)
        return aus

    @property
    def ersparnis(self) -> float:
        """Wie viel kleiner der Strom geworden ist (1 = nichts gespart)."""
        if self.opus_bytes <= 0:
            return 1.0
        return self.roh_bytes / self.opus_bytes


def _baue(empfaenger: int, rate: int, werte: int, nutz: bytes) -> bytes:
    """Einen Opus-Rahmen mit dem kurzen Kopf bauen (siehe oben).

    `werte` zaehlt ABTASTWERTE, nicht Bytes -- so weiss die Gegenseite,
    wie lang der Block ist, ohne das Opus-Paket aufzumachen.
    """
    if rate % 50:
        raise ValueError(f"Abtastrate {rate} ist nicht durch 50 teilbar")
    return _OPUS.pack(OPUS_MARKE, rate // 50, werte, empfaenger, 1) + nutz


def lies_kopf(roh: bytes) -> tuple[int, int, int] | None:
    """Gegenstueck zu _baue -- fuer den Pruefstand und die Gegenprobe.

    Gibt (Rate, Werte, Empfaenger) zurueck, oder None, wenn das kein
    Opus-Rahmen von uns ist.
    """
    if len(roh) < OPUS_KOPF or roh[:4] != OPUS_MARKE:
        return None
    _, rate50, werte, empfaenger, _kanaele = _OPUS.unpack_from(roh, 0)
    return rate50 * 50, werte, empfaenger
