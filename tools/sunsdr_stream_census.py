#!/usr/bin/env python3
"""sunsdr_stream_census.py  (Longpath)

Zaehlt den Empfangsstrom einer SunSDR2 QRP aus und beantwortet die eine
Frage, an der sich am 2026-09-23 Messung und Hoereindruck widersprochen
haben:

    Schickt das Geraet jeden Block wirklich achtmal -- oder tragen die
    acht Pakete mit derselben Folgenummer verschiedene Daten?

Der Widerspruch steht im Quelltext an der Stelle, wo der
Wiederholungsfilter war (SunSdrRadioConnection::processStreamDatagram).
Kurz: mit Filter und 48 kHz stimmt die Rechnung, aber das Rauschen
klingt am Geraet falsch; ohne Filter mit 192 kHz klingt es richtig.
Genau eine der beiden Deutungen kann stimmen:

    A) 240 Bloecke/s, jeder achtmal wiederholt   -> 48 000 Proben/s
    B) 1920 Bloecke/s, die Folgenummer zaehlt
       nur jeden achten Block                    -> 384 000 Proben/s

Der Unterschied ist an EINER Stelle sichtbar: sind die acht Nutzlasten
zu einer Folgenummer bytegleich (A) oder verschieden (B)? Das hier
vergleicht sie alle miteinander -- nicht nur mit dem Vorgaenger. Genau
dieser Fehler hat die Diagnosezeile im Treiber unbrauchbar gemacht: die
Kopien kommen VERSCHRAENKT mit den Nachbarbloecken, also ist das
unmittelbar vorige Paket nie die Kopie.

Ausserdem im Bericht, weil beides bisher niemand angesehen hat:
  * Byte 3 des Kopfes -- der Treiber liest es nicht. Wenn die acht
    Pakete Teilstuecke eines groesseren Blocks sind, steht die
    Teilnummer vermutlich dort.
  * Byte 8 und 9, die der Treiber liest aber nicht auswertet.

Es SENDET NICHTS. Es liest nur mit (tcpdump auf der Schnittstelle --
ein fremdes UDP-Gespraech laesst sich nicht dadurch mithoeren, dass man
denselben Port bindet).

    1. Longpath starten und mit der QRP verbinden, wie immer.
    2. sudo python3 tools/sunsdr_stream_census.py --seconds 30
    3. Den Bericht am Ende lesen. Hoeren muss dabei niemand.

Der Kopf eines IQ-Pakets steht in src/core/sunsdr/SunSdrProtocol.h:
10 Byte, [0] magic0 (QRP: 0x03), [1] 0xFF, [2] Opcode (0xFE = RX),
[4:5] Laenge, [6:7] Folgenummer, [8] und [9] unbenannt. Danach kommen
1200 Byte Nutzlast = 200 Probenpaare zu je 6 Byte.
"""

import argparse
import collections
import re
import subprocess
import sys
import time

MAGIC1 = 0xFF
OP_RX_IDLE = 0xFE
HEADER = 10
PAYLOAD = 1200
PACKET = HEADER + PAYLOAD


def datagrams(proc):
    """Liest tcpdumps -x-Ausgabe und gibt je Datagramm (Zeit, Bytes)."""
    cur = []
    ts = ""
    for raw in proc.stdout:
        line = raw.rstrip("\n")
        if not line:
            continue
        if not line.startswith("\t") and not line.startswith(" "):
            if cur:
                yield ts, bytes.fromhex("".join(cur))
                cur = []
            m = re.match(r"^(\d\d:\d\d:\d\d\.\d+)", line)
            ts = m.group(1) if m else ""
            continue
        for word in line.split()[1:]:
            if re.fullmatch(r"[0-9a-f]{2,4}", word):
                cur.append(word)
    if cur:
        yield ts, bytes.fromhex("".join(cur))


# Wie viel Nutzlast mindestens da sein muss, damit ein Paket zaehlt.
# Bei --snap wird abgeschnitten; 64 Byte sind 10 Probenpaare und genug,
# um zwei Bloecke auseinanderzuhalten (IQ-Rauschen ist nie gleich).
MIN_PAYLOAD = 64


def carve(data, magic0):
    """Schneidet das IQ-Paket aus dem Datagramm heraus.

    Mit `-i any` legt macOS einen PKTAP-Kopf davor, dessen Laenge nicht
    fest ist; darum wird die Marke gesucht statt gezaehlt.

    Ein Treffer zaehlt nur, wenn danach ein vollstaendiger Kopf steht,
    der Opcode 0xFE lautet UND das Laengenfeld 1200 sagt. Die Marke
    allein ist zu schwach -- zwei zufaellige Bytes kommen in 1200 Byte
    Rauschen staendig vor. Die Nutzlast DARF kuerzer sein als 1200: mit
    einer kleinen tcpdump-Schnittlaenge (-s) passt der Mitschnitt in
    einen Bruchteil des Platzes, und fuer die Frage, ob zwei Bloecke
    dieselben Bytes tragen, genuegt der Anfang.
    """
    start = 0
    while True:
        i = data.find(bytes([magic0, MAGIC1]), start)
        if i < 0:
            return None
        rest = data[i:]
        if (len(rest) >= HEADER + MIN_PAYLOAD
                and rest[2] == OP_RX_IDLE
                and (rest[4] | (rest[5] << 8)) == PAYLOAD):
            return rest[:PACKET]
        start = i + 1


# ── pcap/pcapng lesen ───────────────────────────────────────────────────
#
# Eine Aufzeichnung mit `tcpdump -w` ist um ein Vielfaches kleiner und
# schneller einzulesen als dieselben Pakete als Hexziffern, und sie
# bleibt liegen: dieselbe Minute am Geraet laesst sich spaeter noch
# einmal auswerten, ohne das Geraet noch einmal zu brauchen. macOS'
# tcpdump schreibt klassisches pcap; Wireshark schreibt pcapng. Beides
# wird hier gelesen, ohne fremde Pakete.

def _pcapPackets(fh):
    """(Zeit in Sekunden, Rohbytes ab Linkschicht) je Paket."""
    head = fh.read(4)
    if len(head) < 4:
        return
    magic = int.from_bytes(head, "little")
    if magic in (0x0A0D0D0A,):
        yield from _pcapngPackets(fh, head)
        return
    if magic in (0xA1B2C3D4, 0xA1B23C4D):
        endian, nano = "little", (magic == 0xA1B23C4D)
    elif magic in (0xD4C3B2A1, 0x4D3CB2A1):
        endian, nano = "big", (magic == 0x4D3CB2A1)
    else:
        raise SystemExit("Das ist keine pcap-Datei (Magie %08x)." % magic)
    rest = fh.read(20)
    link = int.from_bytes(rest[16:20], endian)
    while True:
        hdr = fh.read(16)
        if len(hdr) < 16:
            return
        ts = int.from_bytes(hdr[0:4], endian)
        frac = int.from_bytes(hdr[4:8], endian)
        caplen = int.from_bytes(hdr[8:12], endian)
        data = fh.read(caplen)
        if len(data) < caplen:
            return
        yield ts + frac / (1e9 if nano else 1e6), link, data


def _pcapngPackets(fh, head):
    """Nur so viel pcapng, wie tcpdump/Wireshark hier schreiben."""
    endian = "little"
    link = 1
    fh.seek(0)
    while True:
        bh = fh.read(8)
        if len(bh) < 8:
            return
        btype = int.from_bytes(bh[0:4], endian)
        blen = int.from_bytes(bh[4:8], endian)
        if blen < 12:
            return
        body = fh.read(blen - 12)
        fh.read(4)
        if btype == 0x00000001 and len(body) >= 4:          # Interface Description
            link = int.from_bytes(body[0:2], endian)
        elif btype == 0x00000006 and len(body) >= 20:       # Enhanced Packet
            hi = int.from_bytes(body[4:8], endian)
            lo = int.from_bytes(body[8:12], endian)
            caplen = int.from_bytes(body[12:16], endian)
            ts = ((hi << 32) | lo) / 1e6
            yield ts, link, body[20:20 + caplen]


def _udpPayload(link, data):
    """Nutzbytes eines UDP-Datagramms, oder None. Nur IPv4/UDP."""
    if link == 0:            # DLT_NULL (loopback)
        off = 4
    elif link == 1:          # DLT_EN10MB
        if len(data) < 14 or data[12:14] != b"\x08\x00":
            return None
        off = 14
    elif link == 113:        # DLT_LINUX_SLL
        off = 16
    elif link == 12 or link == 101:   # DLT_RAW
        off = 0
    elif link == 276:        # DLT_LINUX_SLL2
        off = 20
    else:
        off = 0
    if len(data) < off + 20:
        return None
    ip = data[off:]
    if (ip[0] >> 4) != 4:
        return None
    ihl = (ip[0] & 0x0F) * 4
    if ip[9] != 17 or len(ip) < ihl + 8:
        return None
    return ip[ihl + 8:]


def pcapDatagrams(path):
    """(Zeitmarke als Text, UDP-Nutzbytes) je Datagramm der Datei."""
    import datetime
    with open(path, "rb") as fh:
        for ts, link, data in _pcapPackets(fh):
            pl = _udpPayload(link, data)
            if pl is None:
                continue
            t = datetime.datetime.fromtimestamp(ts).strftime("%H:%M:%S.%f")
            yield t, pl


class FileLike:
    """Gibt datagrams() dasselbe .stdout wie ein Popen-Objekt."""
    def __init__(self, fh):
        self.stdout = fh
    def terminate(self):
        pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--iface", default="any")
    ap.add_argument("--seconds", type=int, default=30)
    ap.add_argument("--magic0", type=lambda x: int(x, 0), default=0x03,
                    help="0x03 = QRP, 0x32 = DX, 0x01 = PRO")
    ap.add_argument("--port", type=int, default=50002)
    ap.add_argument("--snap", type=int, default=320,
                    help="tcpdumps Schnittlaenge. 320 = IP/UDP-Kopf, der "
                         "10-Byte-Kopf und 282 Byte Nutzlast -- genug, um "
                         "zwei Bloecke auseinanderzuhalten, und wenig genug, "
                         "dass dieses Programm bei 1 900 Paketen/s mitkommt. "
                         "2000 nimmt alles (aber rechne mit Verlusten).")
    ap.add_argument("--pcap", default="",
                    help="eine mit `tcpdump -w` gesicherte Aufzeichnung "
                         "auswerten statt selbst mitzulesen")
    ap.add_argument("--src", default="",
                    help="nur Pakete VON dieser Adresse zaehlen (die QRP). "
                         "Ohne das zaehlt das Werkzeug beide Richtungen: "
                         "ein Host, der jeden Block mit derselben "
                         "Folgenummer beantwortet (ExpertSDR2, Longpath "
                         "seit 2026-09-24), erscheint dann als 'zwei "
                         "verschiedene Pakete je Nummer'. Genau das hat die "
                         "Messung vom 2026-09-23 in die Irre gefuehrt. "
                         "Nur beim Live-Mitlesen, nicht bei --pcap/--from-file.")
    ap.add_argument("--from-file", dest="fromFile", default="",
                    help="statt tcpdump eine schon gesicherte -x-Ausgabe "
                         "auswerten (tcpdump ... > datei)")
    args = ap.parse_args()

    if args.pcap:
        censusFromPcap(args)
        return

    if args.fromFile:
        with open(args.fromFile, "r") as fh:
            census(FileLike(fh), args)
        return

    cmd = ["tcpdump", "-i", args.iface, "-n", "-l", "-U", "-x",
           "-s", str(args.snap),
           ("udp port %d and src host %s" % (args.port, args.src))
           if args.src else ("udp port %d" % args.port)]
    if not args.src:
        print("# ACHTUNG: beide Richtungen werden gezaehlt -- mit --src "
              "<Adresse der QRP> nur ihre eigenen Pakete.", flush=True)
    print("# " + " ".join(cmd), flush=True)
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.DEVNULL, text=True)

    census(proc, args)


def censusFromPcap(args):
    """Dieselbe Auszaehlung, nur aus einer Datei statt vom Draht."""
    bySeq = collections.OrderedDict()
    order = []
    total = 0
    firstTs = lastTs = None
    for ts, pl in pcapDatagrams(args.pcap):
        pkt = carve(pl, args.magic0)
        if pkt is None:
            continue
        seq = pkt[6] | (pkt[7] << 8)
        total += 1
        if seq not in bySeq:
            bySeq[seq] = []
            order.append(seq)
        bySeq[seq].append((pkt[:HEADER], pkt[HEADER:]))
        if firstTs is None:
            firstTs = ts
        lastTs = ts
    elapsed = spanSeconds(firstTs, lastTs) or 1e-9
    report(bySeq, order, total, elapsed)


def census(proc, args):
    # Je Folgenummer alle Nutzlasten und Koepfe sammeln. Ein Ring reicht
    # nicht: die Frage ist gerade, ob spaete Kopien anders aussehen.
    bySeq = collections.OrderedDict()
    order = []            # Folgenummern in der Reihenfolge ihres ERSTEN Auftretens
    total = 0
    started = time.time()

    firstTs = lastTs = None
    try:
        for ts, data in datagrams(proc):
            pkt = carve(data, args.magic0)
            if pkt is None:
                continue
            seq = pkt[6] | (pkt[7] << 8)
            total += 1
            if seq not in bySeq:
                bySeq[seq] = []
                order.append(seq)
            bySeq[seq].append((pkt[:HEADER], pkt[HEADER:]))
            if firstTs is None:
                firstTs = ts
            lastTs = ts
            if time.time() - started >= args.seconds:
                break
    except KeyboardInterrupt:
        pass
    finally:
        proc.terminate()

    # Die Dauer aus tcpdumps eigenen Zeitmarken: bei --from-file ist die
    # Uhr dieses Programms nicht die Uhr der Aufzeichnung.
    elapsed = spanSeconds(firstTs, lastTs)
    if elapsed is None:
        elapsed = max(time.time() - started, 1e-9)
    report(bySeq, order, total, elapsed)


def spanSeconds(first, last):
    """Sekunden zwischen zwei tcpdump-Zeitmarken (hh:mm:ss.ffffff)."""
    def asSeconds(t):
        h, m, rest = t.split(":")
        return int(h) * 3600 + int(m) * 60 + float(rest)
    if not first or not last:
        return None
    try:
        d = asSeconds(last) - asSeconds(first)
    except ValueError:
        return None
    if d < 0:
        d += 24 * 3600          # ueber Mitternacht
    return d if d > 1e-6 else None


def report(bySeq, order, total, elapsed):
    print("\n# ── Bericht ──────────────────────────────────────────────")
    print("# Dauer            %.1f s" % elapsed)
    print("# Pakete           %d  (%.0f/s)" % (total, total / elapsed))
    print("# Folgenummern     %d  (%.0f/s)" % (len(bySeq), len(bySeq) / elapsed))
    if not bySeq:
        print("#\n# Nichts gesehen. Laeuft die Verbindung? Braucht tcpdump sudo?")
        return

    counts = collections.Counter(len(v) for v in bySeq.values())
    print("# Pakete je Folgenummer:",
          ", ".join("%dx bei %d Nummern" % (k, counts[k]) for k in sorted(counts)))

    # ── Die eigentliche Frage ───────────────────────────────────────
    groups = ident = differ = 0
    firstDiff = collections.Counter()
    examples = []
    for seq, items in bySeq.items():
        if len(items) < 2:
            continue
        groups += 1
        # Nur so weit vergleichen, wie bei ALLEN Kopien Nutzlast da ist
        # -- bei abgeschnittenem Mitschnitt sonst ein Scheinunterschied.
        n = min(len(pl) for _h, pl in items)
        base = items[0][1][:n]
        allSame = True
        for _hdr, pl in items[1:]:
            if pl[:n] == base:
                continue
            allSame = False
            for i, (a, b) in enumerate(zip(base, pl[:n])):
                if a != b:
                    firstDiff[i] += 1
                    break
        if allSame:
            ident += 1
        else:
            differ += 1
            if len(examples) < 3:
                examples.append(seq)

    print("#")
    print("# Folgenummern mit mehr als einem Paket: %d" % groups)
    print("#   alle Nutzlasten bytegleich:   %d" % ident)
    print("#   mindestens eine VERSCHIEDEN:  %d" % differ)
    if firstDiff:
        top = firstDiff.most_common(5)
        print("#   erste abweichende Stelle (Byte im 1200er-Block):",
              ", ".join("%d (%dx)" % (o, n) for o, n in top))
    if examples:
        print("#   Beispiele:", ", ".join(str(s) for s in examples))

    # ── Die Koepfe: unterscheiden sich die Kopien irgendwo? ─────────
    for name, idx in (("Byte 3", 3), ("Byte 8", 8), ("Byte 9", 9)):
        values = collections.Counter()
        varying = 0
        for items in bySeq.values():
            seen = {h[idx] for h, _p in items}
            values.update(seen)
            if len(seen) > 1:
                varying += 1
        vs = ", ".join("0x%02x (%dx)" % (v, n) for v, n in values.most_common(6))
        print("# %s: %s%s" % (name, vs,
              "  -- unterscheidet die Kopien in %d Gruppen" % varying if varying else ""))

    # ── Laufen die Folgenummern lueckenlos? ─────────────────────────
    gaps = back = 0
    for a, b in zip(order, order[1:]):
        d = (b - a) & 0xFFFF
        if d == 1:
            continue
        if d > 0x8000:
            back += 1
        else:
            gaps += 1
    print("# Erstauftritte: %d Schritte, davon %d Spruenge vorwaerts, %d rueckwaerts"
          % (max(len(order) - 1, 0), gaps, back))

    # ── Schlusssatz ─────────────────────────────────────────────────
    print("#")
    if differ == 0 and groups > 0:
        print("# DEUTUNG A: die Wiederholungen tragen dieselben Bytes.")
        print("#   %.0f verschiedene Bloecke/s x 200 Probenpaare = %.0f Proben/s."
              % (len(bySeq) / elapsed, len(bySeq) / elapsed * 200))
    elif differ > 0:
        print("# DEUTUNG B: Pakete mit derselben Folgenummer tragen")
        print("#   VERSCHIEDENE Daten -- die Folgenummer zaehlt nicht je Paket.")
        print("#   %.0f Pakete/s x 200 Probenpaare = %.0f Proben/s."
              % (total / elapsed, total / elapsed * 200))
        print("#   Dann war der Wiederholungsfilter ein Datenverlust, und")
        print("#   der Hoereindruck des Betreibers hatte recht.")
    else:
        print("# Keine Folgenummer kam mehrfach -- der Strom ist bereits dicht.")


if __name__ == "__main__":
    main()
