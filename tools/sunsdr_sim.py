#!/usr/bin/env python3
"""sunsdr_sim.py  (Longpath)

Ein Messgeraet, kein Funkgeraet.

Stellt eine SunSDR2 QRP so weit nach, wie Longpath sie braucht: sie
antwortet auf die Suchanfrage, nimmt den Zustandsrahmen entgegen, schickt
danach einen echten IQ-Strom und laesst ihn verstummen, wenn das Lebens-
zeichen ausbleibt — genau die Eigenschaft, an der der Treiber schon
einmal gescheitert ist.

Alles hier steht aus Longpaths EIGENEN Quellen: src/core/sunsdr/
SunSdrProtocol.{h,cpp} (Rahmenaufbau, Probenformat, Portnummern) und
docs/architecture/2026-08-26-sunsdr-connection-plan.md (der am Geraet
bewiesene Ablauf). Kein fremder Code, keine Firmware, nichts aus
ExpertSDR2.

Longpath-eigen. Nichts daraus stammt aus ExpertSDR2 oder einer anderen
fremden Quelle: der Rahmenaufbau steht in unseren eigenen Dateien
(src/core/sunsdr/SunSdrProtocol.{h,cpp}), der am Geraet bewiesene Ablauf
in docs/architecture/2026-08-26-sunsdr-connection-plan.md.

Anleitung: docs/development/sunsdr-simulator-workbench.md

Start:
    python3 tools/sunsdr_sim.py      # QRP, Ton bei +10 kHz
    python3 sunsdr_sim.py --no-keepalive-timeout
    python3 sunsdr_sim.py --tone-offset 25000 --tone-dbfs -40

Der Simulator haelt sich an Longpaths Sicht der Sache:
  Steuerkanal  UDP 50001   Suche, Zustand, Frequenz, MOX
  Datenstrom   UDP 50002   1210-Byte-Pakete, 200 Proben je Paket
  Rate         312 500 Hz  → 1562,5 Pakete/s
"""

import argparse
import math
import os
import random
import socket
import struct
import sys
import time

MAGIC0_QRP = 0x03
MAGIC1 = 0xFF
CTRL_PORT = 50001
STREAM_PORT = 50002
IQ_HDR = 10
IQ_PAYLOAD = 1200
IQ_COMPLEX = 200
SAMPLE_RATE = 312500.0

OP_STATE_SYNC = 0x01
OP_FREQ = 0x08
OP_MOX = 0x06
OP_IQ_RX_IDLE = 0xFE
OP_IQ_TX_ACTIVE = 0xFD


def log(msg):
    print("%s %s" % (time.strftime("%H:%M:%S"), msg), flush=True)


def beacon_frame():
    """Die Antwort auf die Suchanfrage.

    Longpath prueft nur die ersten drei Bytes (magic0, magic1, opcode
    0x01) -- ausdruecklich als Byte-Vergleich, weil Byte 3 des echten
    Beacons 0x1a ist und damit nicht in den allgemeinen Kopf passt
    (SunSdrRadioConnection.cpp, processControlDatagram). Genau diese
    Form wird hier nachgebaut.
    """
    f = bytearray(26)
    f[0] = MAGIC0_QRP
    f[1] = MAGIC1
    f[2] = 0x01
    f[3] = 0x1A
    # Ein paar Bytes, die nach Seriennummer/Version aussehen. Longpath
    # liest sie heute nicht; sie stehen hier, damit ein Mitschnitt des
    # Simulators nicht wie ein leerer Rahmen aussieht.
    f[8] = 0x51  # 'Q'
    f[9] = 0x52  # 'R'
    f[10] = 0x50  # 'P'
    f[11] = 0x01  # Fassung
    return bytes(f)


def iq_header(seq, opcode=OP_IQ_RX_IDLE, b8=0, b9=0):
    return struct.pack("<BBBBHHBB", MAGIC0_QRP, MAGIC1, opcode, 0xFF,
                       IQ_PAYLOAD, seq & 0xFFFF, b8, b9)


def make_payload(tone_offset_hz, tone_dbfs, noise_dbfs, phase, n=IQ_COMPLEX):
    """Ein Paket voll Proben: ein Ton plus Rauschen.

    Probenformat wie decodeIqSamples() es liest: je Probe sechs Bytes,
    erst Q (drei Bytes, niederwertiges zuerst), dann I. Jede Gruppe ist
    ein 24-Bit-Wert, den der Treiber um acht Bit nach oben schiebt.
    """
    out = bytearray(IQ_PAYLOAD)
    amp = (10.0 ** (tone_dbfs / 20.0)) * 8388607.0
    nz = (10.0 ** (noise_dbfs / 20.0)) * 8388607.0
    dphi = 2.0 * math.pi * tone_offset_hz / SAMPLE_RATE
    for k in range(n):
        phase += dphi
        i = int(amp * math.cos(phase) + random.gauss(0.0, nz))
        q = int(amp * math.sin(phase) + random.gauss(0.0, nz))
        i = max(-8388608, min(8388607, i))
        q = max(-8388608, min(8388607, q))
        b = k * 6
        out[b + 0] = q & 0xFF
        out[b + 1] = (q >> 8) & 0xFF
        out[b + 2] = (q >> 16) & 0xFF
        out[b + 3] = i & 0xFF
        out[b + 4] = (i >> 8) & 0xFF
        out[b + 5] = (i >> 16) & 0xFF
    return bytes(out), phase % (2.0 * math.pi)


def decode_freq(payload):
    """Longpaths Kandidaten-Kodierung: acht Bytes, niederwertig zuerst,
    Wert durch zehn."""
    if len(payload) < 8:
        return None
    return struct.unpack("<Q", payload[:8])[0] // 10


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tone-offset", type=float, default=10000.0)
    ap.add_argument("--tone-dbfs", type=float, default=-30.0)
    ap.add_argument("--noise-dbfs", type=float, default=-80.0)
    ap.add_argument("--repeat", type=int, default=8,
                    help="wie oft jeder Block wiederholt wird (am Geraet "
                         "gemessen: 8; 1 = keine Wiederholung)")
    ap.add_argument("--block-rate", type=float, default=240.0,
                    help="neue Bloecke je Sekunde (am Geraet: 240 -> "
                         "48 000 Proben/s)")
    ap.add_argument("--keepalive-timeout", type=float, default=8.0,
                    help="Sekunden ohne Lebenszeichen, nach denen der "
                         "Strom verstummt (0 = nie)")
    ap.add_argument("--bind", default="127.0.0.1")
    args = ap.parse_args()

    ctrl = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    ctrl.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    ctrl.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    ctrl.bind((args.bind, CTRL_PORT))
    ctrl.setblocking(False)

    strm = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    strm.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    strm.bind((args.bind, STREAM_PORT))
    strm.setblocking(False)

    log("SunSDR2 QRP (Messgeraet) — Steuerung %s:%d, Strom %s:%d"
        % (args.bind, CTRL_PORT, args.bind, STREAM_PORT))
    log("Ton %+.0f Hz bei %.0f dBFS, Rauschen %.0f dBFS, Rate %.0f Hz"
        % (args.tone_offset, args.tone_dbfs, args.noise_dbfs, SAMPLE_RATE))

    peer = None            # Adresse des Programms (Steuerkanal)
    stream_peer = None     # (Adresse, Port), von dem das Lebenszeichen kam
    streaming = False
    seq = 0
    phase = 0.0
    last_keepalive = 0.0
    next_pkt = time.monotonic()
    # Am Geraet gemessen (2026-09-23): neue Bloecke alle 4,17 ms (240/s),
    # jeder achtmal wiederholt -> 1920 Pakete/s auf dem Draht, aber nur
    # 48 000 Proben/s an echten Daten.
    pkt_interval = 1.0 / args.block_rate
    payload, phase = make_payload(args.tone_offset, args.tone_dbfs,
                                  args.noise_dbfs, phase)
    payload_age = time.monotonic()
    sent = 0
    last_status = 0.0
    last_report = time.monotonic()

    while True:
        now = time.monotonic()

        # ── Steuerkanal ──────────────────────────────────────────────
        try:
            while True:
                data, addr = ctrl.recvfrom(2048)
                if len(data) < 3 or data[0] != MAGIC0_QRP or data[1] != MAGIC1:
                    log("Steuerung: fremdes Paket von %s:%d verworfen (%d B)"
                        % (addr[0], addr[1], len(data)))
                    continue
                op = data[2]
                if op == 0x00:
                    log("Steuerung: Suchanfrage von %s:%d -> Beacon"
                        % (addr[0], addr[1]))
                    ctrl.sendto(beacon_frame(), addr)
                    peer = addr[0]
                elif op == OP_STATE_SYNC:
                    peer = addr[0]
                    if not streaming:
                        streaming = True
                        last_keepalive = now
                        next_pkt = now
                        log("Steuerung: Zustandsrahmen (0x01) von %s -> "
                            "Strom an" % peer)
                elif op == OP_FREQ:
                    hz = decode_freq(data[18:]) if len(data) > 18 else None
                    log("Steuerung: FREQUENZ (0x08) -> %s"
                        % ("%d Hz" % hz if hz else "unlesbar"))
                elif op == OP_MOX:
                    log("Steuerung: MOX (0x06) payload=%s"
                        % data[18:22].hex())
                else:
                    log("Steuerung: Opcode 0x%02x (%d B)" % (op, len(data)))
        except BlockingIOError:
            pass

        # ── Lebenszeichen auf dem Datenstrom ─────────────────────────
        try:
            while True:
                data, addr = strm.recvfrom(4096)
                if len(data) >= 3 and data[2] in (OP_IQ_RX_IDLE, OP_IQ_TX_ACTIVE):
                    last_keepalive = now
                    if stream_peer != addr:
                        stream_peer = addr
                        log("Strom: Gegenstelle gelernt -> %s:%d"
                            % (addr[0], addr[1]))
                    if data[2] == OP_IQ_TX_ACTIVE:
                        log("Strom: TX-Paket empfangen (%d B) — der "
                            "Sendeweg lebt" % len(data))
        except BlockingIOError:
            pass

        # ── Strom ────────────────────────────────────────────────────
        if streaming and args.keepalive_timeout > 0 and \
                now - last_keepalive > args.keepalive_timeout:
            log("Strom: %.0f s ohne Lebenszeichen -> verstummt (so wie das "
                "echte Geraet)" % args.keepalive_timeout)
            streaming = False

        if streaming and stream_peer is None and peer:
            # Solange kein Lebenszeichen kam, wissen wir nicht, WOHIN.
            # Das echte Geraet kennt den festen Port 50002; auf einer
            # Maschine, auf der Messgeraet UND Programm laufen, ist der
            # schon vergeben, also lernen wir ihn aus dem ersten
            # Lebenszeichen (der Treiber schickt es alle 2 s).
            pass

        if streaming and stream_peer:
            # Nicht jedes Paket neu rechnen: der Ton laeuft weiter, aber
            # 1562 Pakete je Sekunde einzeln zu wuerfeln kostet mehr, als
            # die Werkbank braucht. Alle 200 ms ein frischer Block.
            if now - payload_age > 0.2:
                payload, phase = make_payload(args.tone_offset, args.tone_dbfs,
                                              args.noise_dbfs, phase)
                payload_age = now
            while next_pkt <= now:
                pkt = iq_header(seq) + payload
                for _ in range(max(1, args.repeat)):
                    strm.sendto(pkt, stream_peer)
                    sent += 1
                seq = (seq + 1) & 0xFFFF
                next_pkt += pkt_interval
            # Nicht endlos nachholen, wenn die Maschine kurz stockt.
            if next_pkt < now - 0.1:
                next_pkt = now

        # Statusrahmen, 20/s -- so wie das echte Geraet (Temperatur als
        # float bei Offset 15 und 19, siehe Mitschnitt vom 2026-09-23).
        if streaming and stream_peer and now - last_status >= 0.05:
            last_status = now
            st = bytearray(77)
            st[0:4] = bytes([MAGIC0_QRP, MAGIC1, 0x00, 0x1f])
            st[6:10] = int(now * 1e6).to_bytes(8, "little")[:4]   # schneller Zaehler
            struct.pack_into("<f", st, 15, 36.5)                  # Temperatur 1
            struct.pack_into("<f", st, 19, 27.0)                  # Temperatur 2
            struct.pack_into("<f", st, 31, 1.0)
            strm.sendto(bytes(st), stream_peer)

        if now - last_report >= 5.0:
            if streaming:
                log("Strom: %d Pakete in 5 s (%.0f/s, %.1f MB/s) — davon "
                    "%.0f/s echte Bloecke" % (sent, sent / 5.0,
                    sent * 1210 / 5.0 / 1e6, sent / 5.0 / max(1, args.repeat)))
            sent = 0
            last_report = now

        time.sleep(0.0002)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        log("Ende")
