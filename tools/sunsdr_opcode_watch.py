#!/usr/bin/env python3
"""sunsdr_opcode_watch.py  (Longpath)

Schaut zu, welche Steuerrahmen zwischen einem Programm und einer
SunSDR2 QRP hin und her gehen, und schreibt sie so auf, dass man sie
hinterher einem Handgriff zuordnen kann.

Gedacht fuer die Bank ohne Antenne: acht Opcodes sind bis heute nicht
zugeordnet (0x03, 0x0c, 0x0d, 0x0f, 0x11, 0x13, 0x16, 0x1c), und die
meisten davon gehoeren zu Dingen, die den Empfang betreffen -- Filter,
Vorverstaerker, Daempfung, Abtastrate. Wer ExpertSDR2 bedient, waehrend
das hier laeuft, bekommt eine Liste "um 12:03:17 kam Opcode 0x0d mit
diesen vier Bytes" und kann sie neben seine eigenen Notizen legen.

Es SENDET NICHTS. Es liest nur mit (tcpdump auf der Schnittstelle --
UDP ist nicht mitlesbar, indem man denselben Port bindet).

    sudo python3 tools/sunsdr_opcode_watch.py --iface en0 --seconds 120
    sudo python3 tools/sunsdr_opcode_watch.py --iface en0 --mark

Mit --mark wartet das Programm nach jedem Druck auf die Eingabetaste
auf eine Beschriftung und schreibt sie zwischen die Rahmen -- so
entsteht das Protokoll direkt beim Bedienen:

    [12:03:15] MARKE: Vorverstaerker ein
    12:03:17.412  ->  op=0x0d  len=4  payload=01000000
    [12:03:22] MARKE: Vorverstaerker aus
    12:03:23.008  ->  op=0x0d  len=4  payload=00000000

Mit --pcap wertet es eine schon gesicherte Aufzeichnung aus
(`sudo tcpdump -i en9 -s 0 -w sitzung.pcap host <geraet>`) statt selbst
mitzulesen -- dann bleibt die Minute am Geraet liegen und laesst sich
spaeter noch einmal ansehen. Mit --full steht die GANZE Nutzlast da,
nicht nur die ersten Bytes: am 2026-08-26 schickte ExpertSDR2 beim
Verbinden ein rund 1,2 kB grosses Paket, und zwar zweimal, und darin
steht vermutlich, was Longpath dem Geraet bis heute nie sagt.

Der Kopf eines Steuerrahmens steht in src/core/sunsdr/SunSdrProtocol.h:
18 Byte, [0] magic0 (QRP: 0x03), [1] 0xFF, [2] Opcode, [4:5] Laenge,
[6:7] Unterindex. Alles danach ist Nutzlast.
"""

import argparse
import datetime
import re
import subprocess
import sys
import threading

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


def _udpPayload(link, data, withPeers=False):
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
    body = ip[ihl + 8:]
    if not withPeers:
        return body
    src = ".".join(str(b) for b in ip[12:16])
    dst = ".".join(str(b) for b in ip[16:20])
    sport = int.from_bytes(ip[ihl:ihl + 2], "big")
    dport = int.from_bytes(ip[ihl + 2:ihl + 4], "big")
    return body, "%s:%d" % (src, sport), "%s:%d" % (dst, dport)


def pcapDatagrams(path, withPeers=False):
    """(Zeitmarke als Text, UDP-Nutzbytes) je Datagramm der Datei.

    Mit withPeers zusaetzlich (Absender, Empfaenger) als "ip:port" --
    ohne die Richtung laesst sich nicht sagen, ob ein Rahmen eine Frage
    des Programms oder eine Antwort des Geraets ist, und genau daran
    haengt jede Deutung.
    """
    import datetime
    with open(path, "rb") as fh:
        for ts, link, data in _pcapPackets(fh):
            got = _udpPayload(link, data, withPeers)
            if got is None:
                continue
            t = datetime.datetime.fromtimestamp(ts).strftime("%H:%M:%S.%f")
            yield (t,) + got if withPeers else (t, got)

MAGIC1 = 0xFF
KNOWN = {
    0x00: "Suchanfrage (Rundsendung)",
    0x01: "Zustand / Start des Stroms",
    0x04: "Vorverstaerker / Daempfung",
    0x06: "MOX / PTT (Kandidat)",
    0x08: "Frequenz",
    0x15: "Antennenwahl (Kandidat)",
    0x17: "Ansteuerung/Drive (Kandidat)",
    0x24: "PA ein (Kandidat)",
}


def hexdump_lines(proc, magic0, out):
    """Liest tcpdumps -x-Ausgabe und gibt je Datagramm die Nutzbytes."""
    cur = []
    ts = ""
    for raw in proc.stdout:
        line = raw.rstrip("\n")
        if not line:
            continue
        if not line.startswith("\t") and not line.startswith(" "):
            if cur:
                yield ts, cur
                cur = []
            m = re.match(r"^(\d\d:\d\d:\d\d\.\d+)", line)
            ts = m.group(1) if m else ""
            continue
        for word in line.split()[1:]:
            if re.fullmatch(r"[0-9a-f]{2,4}", word):
                cur.append(word)
    if cur:
        yield ts, cur


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--iface", default="en0")
    ap.add_argument("--seconds", type=int, default=0,
                    help="0 = bis Strg-C")
    ap.add_argument("--magic0", type=lambda x: int(x, 0), default=0x03,
                    help="0x03 = QRP, 0x32 = DX, 0x01 = PRO")
    ap.add_argument("--port", type=int, default=50001,
                    help="Steuerport (beide Seiten benutzen ihn)")
    ap.add_argument("--radio", default="",
                    help="Adresse des Funkgeraets, fuer die Richtungspfeile. "
                         "Ohne Angabe aus der Rundsendung erraten.")
    ap.add_argument("--mark", action="store_true",
                    help="Eingabetaste setzt eine Marke ins Protokoll")
    ap.add_argument("--pcap", default="",
                    help="eine mit `tcpdump -w` gesicherte Aufzeichnung "
                         "auswerten statt selbst mitzulesen")
    ap.add_argument("--full", action="store_true",
                    help="die ganze Nutzlast zeigen, nicht nur den Anfang")
    args = ap.parse_args()

    if args.pcap:
        frames = [(ts, pl, src, dst) for ts, pl, src, dst
                  in pcapDatagrams(args.pcap, withPeers=True)]
        radio = args.radio or guessRadio(frames, args.magic0)
        if radio:
            print("# Geraet: %s" % radio)
        seen = {}
        for ts, pl, src, dst in frames:
            emit(ts, pl, args, seen, src, dst, radio)
        summarise(seen)
        return

    cmd = ["tcpdump", "-i", args.iface, "-n", "-l", "-U", "-x", "-s", "2000",
           "udp port %d" % args.port]
    print("# " + " ".join(cmd), flush=True)
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.DEVNULL, text=True)

    if args.mark:
        def marker():
            while True:
                try:
                    label = input()
                except EOFError:
                    return
                print("[%s] MARKE: %s"
                      % (datetime.datetime.now().strftime("%H:%M:%S"),
                         label.strip() or "(ohne Text)"), flush=True)
        threading.Thread(target=marker, daemon=True).start()

    seen = {}
    try:
        for ts, words in hexdump_lines(proc, args.magic0, sys.stdout):
            data = bytes.fromhex("".join(words))
            # IP+UDP-Kopf ueberspringen: 20 + 8 Byte (kein IP-Options-Fall
            # auf diesen Geraeten gesehen).
            if len(data) < 28 + 3:
                continue
            emit(ts, data[28:], args, seen)
    except KeyboardInterrupt:
        pass
    finally:
        proc.terminate()

    summarise(seen)


def emit(ts, pl, args, seen, src="", dst="", radio=""):
    """Einen Steuerrahmen aufschreiben, falls es einer ist.

    Die Richtung steht dabei, weil ohne sie keine Deutung moeglich ist:
    derselbe Opcode heisst in die eine Richtung "sag mir X" oder "stell
    X auf diesen Wert" und in die andere "X ist dieser Wert".
    """
    if len(pl) < 3 or pl[0] != args.magic0 or pl[1] != MAGIC1:
        return
    op = pl[2]
    declared = int.from_bytes(pl[4:6], "little") if len(pl) >= 6 else 0
    body = pl[18:] if len(pl) > 18 else b""
    # Ohne --full nur der Anfang: ein 1,2-kB-Paket als eine Zeile
    # Hexziffern macht das Protokoll unlesbar, und fuer das Zuordnen
    # eines Handgriffs genuegen die ersten Bytes.
    payload = body if args.full else body[:min(max(declared, 8), 64)]
    seen[op] = seen.get(op, 0) + 1
    more = "" if (args.full or len(payload) == len(body)) else "..."
    # Wer mit wem. Ueber den Port geht das NICHT: beide Seiten benutzen
    # 50001 (am 2026-09-23 im Mitschnitt gesehen -- eine Annahme, die
    # sich sofort geraecht hat, weil dann jeder Rahmen wie eine Frage
    # des Programms aussah). Also ueber die Adresse.
    if src and dst and radio:
        arrow = "-->" if dst.split(":")[0] == radio else "<--"
    else:
        arrow = "   "
    print("%s %s op=0x%02x  len=%-4d ganz=%-5d payload=%s%s %s"
          % (ts, arrow, op, declared, len(pl), payload.hex(), more,
             KNOWN.get(op, "?? nicht zugeordnet")), flush=True)


def guessRadio(frames, magic0):
    """Das Geraet ist die Seite, die die Rundsendung NICHT geschickt hat.

    Opcode 0x00 geht als Rundsendung an x.x.x.255 hinaus; ihr Absender
    ist damit das Programm, und der andere Teilnehmer das Geraet.
    """
    program = ""
    hosts = set()
    for _ts, pl, src, dst in frames:
        if len(pl) < 3 or pl[0] != magic0 or pl[1] != MAGIC1:
            continue
        s_ip, d_ip = src.split(":")[0], dst.split(":")[0]
        hosts.add(s_ip)
        hosts.add(d_ip)
        if pl[2] == 0x00 and d_ip.endswith(".255"):
            program = s_ip
    if not program:
        return ""
    rest = [h for h in hosts if h != program and not h.endswith(".255")]
    return rest[0] if len(rest) == 1 else ""


def summarise(seen):
    print("\n# Zusammenfassung")
    for op in sorted(seen):
        print("#   0x%02x  %5d mal   %s" % (op, seen[op], KNOWN.get(op, "??")))


if __name__ == "__main__":
    main()
