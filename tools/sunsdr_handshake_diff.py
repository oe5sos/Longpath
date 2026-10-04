#!/usr/bin/env python3
"""sunsdr_handshake_diff.py  (Longpath)

Was sagt ExpertSDR2 der QRP beim Verbinden, das Longpath ihr nicht sagt?

Das ist die offene Frage hinter der Achtfachung: am 2026-09-23 wurde am
Geraet gemessen, dass die QRP an Longpath jeden Block ACHTMAL schickt
(bytegleich), an ExpertSDR2 am selben Geraet in derselben Stunde ZWEI
VERSCHIEDENE. Rahmenrate bei beiden 240/s. Der einzige bekannte
Unterschied: ExpertSDR2 schickt beim Verbinden rund zwei Dutzend
Steuerrahmen, Longpath einen.

Dieses Werkzeug macht aus einem Mitschnitt zwei Dinge, die man sofort
weiterverwenden kann:

  1. eine lesbare Liste dessen, was in welcher Reihenfolge und mit
     welchem Abstand hinausging -- und was das Geraet darauf
     zurueckgeschickt hat (die Rueckrichtung hat Longpath bis zum
     2026-10-02 ueberhaupt nicht angesehen);
  2. die fertigen Hexlisten fuer LONGPATH_SUNSDR_PRE und
     LONGPATH_SUNSDR_EXTRA, mit denen sich derselbe Ablauf ohne
     Neubau ausprobieren laesst (siehe sendBenchFrames() in
     SunSdrRadioConnection.cpp).

Es wird nichts gesendet und nichts geoeffnet ausser der Datei.

Mitschnitt anlegen (zwei Minuten, Antenne nicht noetig):

    sudo tcpdump -i en0 -s 0 -w expert.pcap host <IP-der-QRP>
    # ExpertSDR2 starten, verbinden lassen, 10 s warten, beenden
    # dann tcpdump mit Strg-C beenden

Auswerten:

    python3 tools/sunsdr_handshake_diff.py expert.pcap
    python3 tools/sunsdr_handshake_diff.py expert.pcap --alle
    python3 tools/sunsdr_handshake_diff.py --selftest

Longpath-eigen. Der Rahmenaufbau steht in src/core/sunsdr/
SunSdrProtocol.{h,cpp}; nichts hier stammt aus ExpertSDR2 oder einer
anderen fremden Quelle -- das Werkzeug liest nur Bytes, die auf dem
eigenen Netz des Betreibers an sein eigenes Geraet gehen.
"""

import argparse
import os
import socket
import struct
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sunsdr_opcode_watch import _pcapPackets, _udpPayload, KNOWN, MAGIC1

CTRL_PORT = 50001
STREAM_PORT = 50002
CTL_HDR = 18

# Was Longpath selbst je schickt, Stand 2026-10-03 (am Geraet
# nachgezaehlt, nicht aus dem Code geraten -- die erste Fassung dieser
# Liste enthielt den Zustandsrahmen falsch): Suchanfrage (0x00),
# Stromstart/STATE_SYNC (0x01), Vorverstaerker/Daempfung (0x04),
# DDC-Frequenz (0x07), VFO-Frequenz (0x08). Alles andere in einem
# Mitschnitt von ExpertSDR2 ist etwas, das wir dem Geraet nie sagen.
LONGPATH_SENDET = {0x00, 0x01, 0x04, 0x07, 0x08}


def udpMitRichtung(path):
    """(Zeit, QuellIP, QuellPort, ZielIP, ZielPort, Nutzbytes) je Datagramm."""
    with open(path, "rb") as fh:
        for ts, link, data in _pcapPackets(fh):
            pl = _udpPayload(link, data)
            if pl is None:
                continue
            # _udpPayload hat die Kopfpruefung schon gemacht; die Adressen
            # holen wir uns hier noch einmal aus demselben Rahmen.
            off = {0: 4, 1: 14, 113: 16, 12: 0, 101: 0, 276: 20}.get(link, 0)
            ip = data[off:]
            ihl = (ip[0] & 0x0F) * 4
            src = socket.inet_ntoa(ip[12:16])
            dst = socket.inet_ntoa(ip[16:20])
            udp = ip[ihl:ihl + 8]
            sport, dport = struct.unpack(">HH", udp[0:4])
            yield ts, src, sport, dst, dport, pl


def kopf(pl):
    """(Opcode, sub, angekuendigte Laenge) oder None, wenn kein Steuerkopf."""
    if len(pl) < CTL_HDR or pl[1] != MAGIC1:
        return None
    op = pl[2]
    laenge = pl[4] | (pl[5] << 8)
    sub = pl[6] | (pl[7] << 8)
    return op, sub, laenge


def deuten(op):
    return KNOWN.get(op, "unbekannt")


def findeRechner(path):
    """Welche Adresse ist der RECHNER (nicht das Geraet)?

    Die Richtung laesst sich NICHT am Zielport ablesen: beide Seiten
    sprechen Port 50001, also ist dport immer 50001. Die erste Fassung
    dieses Werkzeugs tat genau das und hielt deshalb am 2026-10-03 im
    ersten echten Mitschnitt alle zehn Rahmen fuer ausgehend -- auch die
    fuenf Quittungen des Geraets.

    Belastbar ist die Suchanfrage: Opcode 0x00 geht immer VOM Rechner aus.
    Fehlt sie im Mitschnitt, entscheidet der Datenstrom -- die
    1210-Byte-Pakete kommen aus dem Geraet. Bleibt auch das offen, muss es
    --rechner sagen.
    """
    stromQuelle = None
    for ts, src, sport, dst, dport, pl in udpMitRichtung(path):
        k = kopf(pl)
        if k is not None and k[0] == 0x00:
            return src
        if (sport == STREAM_PORT or dport == STREAM_PORT) and len(pl) > 1000:
            stromQuelle = stromQuelle or dst      # Ziel des Stroms = Rechner
    return stromQuelle


def auswerten(path, alle, rechner=None):
    rahmen = []
    ersterStrom = None
    t0 = None
    if rechner is None:
        rechner = findeRechner(path)
    if rechner is None:
        raise SystemExit(
            "Die Richtung laesst sich nicht bestimmen: im Mitschnitt fehlen "
            "sowohl die Suchanfrage (0x00) als auch der Datenstrom. Mit "
            "--rechner <IP> angeben, welche Adresse der Rechner ist.")
    print("Rechner: %s (alles andere ist das Geraet)" % rechner)
    for ts, src, sport, dst, dport, pl in udpMitRichtung(path):
        if dport == STREAM_PORT or sport == STREAM_PORT:
            if ersterStrom is None:
                ersterStrom = ts
            continue
        if dport != CTRL_PORT and sport != CTRL_PORT:
            continue
        if t0 is None:
            t0 = ts
        raus = (src == rechner)
        rahmen.append((ts - t0, raus, pl))

    if not rahmen:
        raise SystemExit(
            "Keine Steuerrahmen auf Port %d in %s gefunden. Lief der "
            "Mitschnitt auf der richtigen Schnittstelle?" % (CTRL_PORT, path))

    grenze = None if ersterStrom is None or t0 is None else ersterStrom - t0

    print("Mitschnitt: %s" % path)
    print("Steuerrahmen: %d (%d hinaus, %d herein)"
          % (len(rahmen),
             sum(1 for _, raus, _ in rahmen if raus),
             sum(1 for _, raus, _ in rahmen if not raus)))
    if grenze is not None:
        print("Erstes Strompaket bei %.3f s -- alles davor ist der Verbindungsablauf."
              % grenze)
    print()
    print("  %-8s %-4s %-6s %-5s %-5s %-28s %s"
          % ("t/s", "Ri", "op", "sub", "len", "Nutzlast (erste 24 Byte)", "Deutung"))

    hinausVorStrom = []
    opcodesHinaus = set()
    opcodesHerein = set()
    for dt, raus, pl in rahmen:
        k = kopf(pl)
        pfeil = "->" if raus else "<-"
        if k is None:
            if alle:
                print("  %-8.3f %-4s %-6s %-5s %-5s %-28s %s"
                      % (dt, pfeil, "?", "-", len(pl),
                         pl[:24].hex(), "kein Steuerkopf"))
            continue
        op, sub, laenge = k
        nutz = pl[CTL_HDR:]
        (opcodesHinaus if raus else opcodesHerein).add(op)
        if raus and (grenze is None or dt <= grenze):
            hinausVorStrom.append(pl)
        if alle or grenze is None or dt <= grenze + 0.5:
            hinweis = deuten(op)
            if laenge != len(nutz):
                hinweis += " [Laenge angekuendigt %d, da %d]" % (laenge, len(nutz))
            print("  %-8.3f %-4s 0x%02x   %-5d %-5d %-28s %s"
                  % (dt, pfeil, op, sub, len(nutz), nutz[:24].hex(), hinweis))

    print()
    neu = sorted(opcodesHinaus - LONGPATH_SENDET)
    print("Opcodes, die ExpertSDR2 hinausschickt und Longpath nie: %s"
          % (", ".join("0x%02x (%s)" % (o, deuten(o)) for o in neu) or "keine"))
    print("Opcodes, die das GERAET zurueckschickt: %s"
          % (", ".join("0x%02x (%s)" % (o, deuten(o)) for o in sorted(opcodesHerein))
             or "keine"))
    print()

    # Der Zustandsrahmen ist bei ExpertSDR2 der vorletzte; Longpath schickt
    # genau ihn und sonst nichts. Darum wird die Liste an der letzten
    # Frequenzmeldung (0x08) geteilt: davor PRE, danach EXTRA. Stimmt die
    # Teilung im Einzelfall nicht, verschiebt man sie von Hand -- die
    # Rohliste steht oben.
    teiler = None
    for i, pl in enumerate(hinausVorStrom):
        k = kopf(pl)
        if k and k[0] == 0x08:
            teiler = i
    pre = hinausVorStrom[:teiler] if teiler is not None else hinausVorStrom
    extra = hinausVorStrom[teiler + 1:] if teiler is not None else []

    print("Zum Ausprobieren ohne Neubau (sendBenchFrames):")
    print()
    print("  export LONGPATH_SUNSDR_PRE=%s" % ",".join(p.hex() for p in pre))
    print()
    print("  export LONGPATH_SUNSDR_EXTRA=%s" % ",".join(p.hex() for p in extra))
    print()
    print("  (%d Rahmen davor, %d danach)" % (len(pre), len(extra)))


# ── Selbsttest: ein kleines pcap selbst bauen und wieder auswerten ──────
#
# Damit das Werkzeug nicht erst am echten Mitschnitt zum ersten Mal
# laeuft. Gebaut wird ein klassisches pcap mit Ethernet/IPv4/UDP.

def _udpPaket(src, dst, sport, dport, nutz):
    udp = struct.pack(">HHHH", sport, dport, 8 + len(nutz), 0) + nutz
    ip = (b"\x45\x00" + struct.pack(">H", 20 + len(udp))
          + b"\x00\x00\x00\x00\x40\x11\x00\x00"
          + socket.inet_aton(src) + socket.inet_aton(dst))
    return b"\xff" * 6 + b"\xaa" * 6 + b"\x08\x00" + ip + udp


def _ctl(op, sub, nutz):
    hdr = bytearray(CTL_HDR)
    hdr[0] = 0x03
    hdr[1] = 0xFF
    hdr[2] = op
    hdr[4] = len(nutz) & 0xFF
    hdr[5] = (len(nutz) >> 8) & 0xFF
    hdr[6] = sub & 0xFF
    hdr[10] = 0x01
    return bytes(hdr) + nutz


def selftest():
    host, radio = "192.0.2.1", "192.0.2.200"
    plan = [
        (0.000, _udpPaket(host, radio, 54000, CTRL_PORT, _ctl(0x00, 0, b""))),
        (0.010, _udpPaket(radio, host, CTRL_PORT, 54000, _ctl(0x01, 0, b"\x01"))),
        (0.020, _udpPaket(host, radio, 54000, CTRL_PORT, _ctl(0x0d, 0, b"\x01\x00\x00\x00"))),
        (0.030, _udpPaket(host, radio, 54000, CTRL_PORT, _ctl(0x1c, 0, b"\x02\x00\x00\x00"))),
        (0.040, _udpPaket(host, radio, 54000, CTRL_PORT, _ctl(0x08, 0, b"\x11\x22\x33\x44"))),
        (0.050, _udpPaket(host, radio, 54000, CTRL_PORT, _ctl(0x13, 0, b"\x07"))),
        (0.100, _udpPaket(radio, host, STREAM_PORT, 54001, b"\x03\xff\xfe\xff" + b"\x00" * 100)),
        (0.200, _udpPaket(host, radio, 54000, CTRL_PORT, _ctl(0x04, 0, b"\x02"))),
    ]
    fd, pfad = tempfile.mkstemp(suffix=".pcap")
    with os.fdopen(fd, "wb") as fh:
        fh.write(struct.pack("<IHHiIII", 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1))
        for t, paket in plan:
            sek = int(t)
            usek = int(round((t - sek) * 1_000_000))
            fh.write(struct.pack("<IIII", 1_700_000_000 + sek, usek,
                                 len(paket), len(paket)))
            fh.write(paket)
    print("Selbsttest -- erzeugtes pcap: %s" % pfad)
    print()
    auswerten(pfad, alle=True)
    os.unlink(pfad)


def vergleiche(a, b):
    """Welche Rahmensorten kommen nur in a, nur in b, oder mit anderer Nutzlast?

    Dafuer gedacht, zwei Mitschnitte desselben Ablaufs gegeneinander zu
    legen -- etwa einmal mit RX2 ein und einmal aus. Der Rahmen, der sich
    dabei unterscheidet, IST der Schalter. Genau so laesst sich ein Opcode
    zuordnen, ohne ihn am Funkgeraet zu erraten.
    """
    def sammle(path):
        rechner = findeRechner(path)
        aus = {}
        for ts, src, sport, dst, dport, pl in udpMitRichtung(path):
            if dport != CTRL_PORT or src != rechner:
                continue          # nur, was der Rechner hinausschickt
            k = kopf(pl)
            if k is None:
                continue
            aus.setdefault((k[0], k[1]), []).append(pl[CTL_HDR:])
        return aus

    A, B = sammle(a), sammle(b)
    nurA = sorted(set(A) - set(B))
    nurB = sorted(set(B) - set(A))
    beide = sorted(set(A) & set(B))

    print("Vergleich:")
    print("  %s" % a)
    print("  %s" % b)
    print()
    for name, menge in (("nur im ersten", nurA), ("nur im zweiten", nurB)):
        if menge:
            print("%s:" % name)
            for op, sub in menge:
                print("  op=0x%02x sub=%d  (%s)" % (op, sub, deuten(op)))
    print()
    print("in beiden, aber mit ANDERER Nutzlast -- das sind die Kandidaten:")
    gefunden = False
    for op, sub in beide:
        wa, wb = set(x.hex() for x in A[(op, sub)]), set(x.hex() for x in B[(op, sub)])
        if wa != wb:
            gefunden = True
            print("  op=0x%02x sub=%d  (%s)" % (op, sub, deuten(op)))
            for x in sorted(wa - wb):
                print("      nur erster:  %s" % x)
            for x in sorted(wb - wa):
                print("      nur zweiter: %s" % x)
    if not gefunden:
        print("  keine -- dieselben Rahmen mit denselben Werten")


def wiederholungen(pfad, geraet=None):
    """Zaehlt bytegleiche Wiederholungen im I/Q-Strom eines Mitschnitts.

    Die Frage dahinter (2026-10-04): Longpath bekommt bei 96 kHz rund
    110 bytegleiche Wiederholungen je Sekunde, bei 48 kHz keine. Drei
    Gegenmassnahmen sind gemessen und wirkungslos. Wiederholt ExpertSDR2
    bei 96 kHz AUCH, ist es die Eigenart des Geraets und kein Mangel von
    Longpath -- und die Frage ist erledigt statt offen.

    Dafuer muss der Mitschnitt von ExpertSDR2 bei 96 kHz stammen.
    """
    import hashlib
    daten = open(pfad, "rb")
    kopf = daten.read(24)
    if len(kopf) < 24:
        print("Datei zu kurz."); return
    magic = struct.unpack("<I", kopf[:4])[0]
    if magic not in (0xa1b2c3d4, 0xd4c3b2a1):
        print("Das sieht nicht nach einem klassischen pcap aus "
              "(pcapng wird hier nicht gelesen).")
        return

    jeKanal = {}
    inhalt = {}
    dubletten = 0
    gesamt = 0
    t0 = t1 = None
    while True:
        rh = daten.read(16)
        if len(rh) < 16:
            break
        ts, tus, incl, _orig = struct.unpack("<IIII", rh)
        d = daten.read(incl)
        if len(d) < incl:
            break
        if incl < 42 or d[12:14] != b"\x08\x00" or d[23] != 17:
            continue
        ihl = (d[14] & 0x0F) * 4
        uo = 14 + ihl
        sp = struct.unpack(">H", d[uo:uo + 2])[0]
        src = ".".join(str(b) for b in d[26:30])
        if sp != 50002:
            continue
        if geraet and src != geraet:
            continue
        nutz = d[uo + 8:]
        # Nur echte IQ-Bloecke: 10 Byte Kopf + 1200 Byte Nutzlast.
        if len(nutz) != 1210 or nutz[2] not in (0xFE, 0xFD):
            continue
        t = ts + tus / 1e6
        if t0 is None:
            t0 = t
        t1 = t
        seq = struct.unpack("<H", nutz[6:8])[0]
        kanal = nutz[9]
        gesamt += 1
        jeKanal[kanal] = jeKanal.get(kanal, 0) + 1
        h = hashlib.blake2b(nutz[10:], digest_size=8).digest()
        schluessel = (kanal, seq)
        if schluessel in inhalt and inhalt[schluessel] == h:
            dubletten += 1
        inhalt[schluessel] = h
        if len(inhalt) > 400000:
            inhalt.clear()
    if gesamt == 0:
        print("Keine I/Q-Bloecke gefunden. Stammt der Mitschnitt vom "
              "Stromport 50002?")
        return
    dauer = max((t1 or 0) - (t0 or 0), 1e-9)
    print("I/Q-Bloecke: %d ueber %.1f s (%.0f/s)"
          % (gesamt, dauer, gesamt / dauer))
    for k in sorted(jeKanal):
        print("   Kanal %d: %d (%.0f/s)" % (k, jeKanal[k], jeKanal[k] / dauer))
    print("bytegleiche Wiederholungen: %d  (%.1f/s, %.1f %% der Bloecke)"
          % (dubletten, dubletten / dauer, 100.0 * dubletten / gesamt))
    print()
    if dubletten / dauer > 20:
        print("-> Das Geraet wiederholt auch hier. Dann ist es seine "
              "Eigenart und kein Mangel von Longpath.")
    else:
        print("-> Praktisch keine Wiederholungen. Dann liegt es NICHT am "
              "Geraet, und Longpath macht etwas anders als dieses "
              "Programm -- der Unterschied steckt im Verbindungsablauf.")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pcap", nargs="?", default="",
                    help="Mitschnitt (pcap oder pcapng)")
    ap.add_argument("--rechner", default="",
                    help="IP des Rechners, falls sie sich nicht aus dem "
                         "Mitschnitt ergibt (siehe findeRechner)")
    ap.add_argument("--alle", action="store_true",
                    help="auch die Rahmen nach dem Verbindungsablauf zeigen")
    ap.add_argument("--vergleich", default="",
                    help="zweiter Mitschnitt: zeigt, welche Rahmen sich "
                         "zwischen beiden unterscheiden (z. B. RX2 ein/aus)")
    ap.add_argument("--wiederholungen", action="store_true",
                    help="zaehlt bytegleiche Wiederholungen im I/Q-Strom -- "
                         "fuer die Frage, ob ExpertSDR2 bei 96 kHz auch "
                         "wiederholt")
    ap.add_argument("--selftest", action="store_true",
                    help="mit einem selbst gebauten Mitschnitt pruefen, "
                         "dass das Werkzeug tut, was es soll")
    args = ap.parse_args()
    if args.selftest:
        selftest()
        return
    if not args.pcap:
        ap.error("Entweder eine pcap-Datei oder --selftest.")
    if args.wiederholungen:
        wiederholungen(args.pcap, args.rechner or None)
        return
    if args.vergleich:
        vergleiche(args.pcap, args.vergleich)
        return
    auswerten(args.pcap, args.alle, args.rechner or None)


if __name__ == "__main__":
    main()
