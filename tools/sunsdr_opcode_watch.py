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
    ap.add_argument("--mark", action="store_true",
                    help="Eingabetaste setzt eine Marke ins Protokoll")
    args = ap.parse_args()

    cmd = ["tcpdump", "-i", args.iface, "-n", "-l", "-U", "-x", "-s", "2000",
           "udp port 50001"]
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
            pl = data[28:]
            if pl[0] != args.magic0 or pl[1] != MAGIC1:
                continue
            op = pl[2]
            declared = int.from_bytes(pl[4:6], "little") if len(pl) >= 6 else 0
            payload = pl[18:18 + max(declared, 8)] if len(pl) > 18 else b""
            seen[op] = seen.get(op, 0) + 1
            print("%s  op=0x%02x  len=%-4d payload=%-24s %s"
                  % (ts, op, declared, payload.hex(),
                     KNOWN.get(op, "?? nicht zugeordnet")), flush=True)
    except KeyboardInterrupt:
        pass
    finally:
        proc.terminate()

    print("\n# Zusammenfassung")
    for op in sorted(seen):
        print("#   0x%02x  %5d mal   %s" % (op, seen[op], KNOWN.get(op, "??")))


if __name__ == "__main__":
    main()
