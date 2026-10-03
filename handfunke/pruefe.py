#!/usr/bin/env python3
"""pruefe.py — misst die Fernbedienungs-Kette von aussen.

Beantwortet die eine Frage, die bei "kein Ton" / "kein Bild" immer zuerst
kommt: liegt es am Programm oder am Draht?

Am 2026-10-03 hat genau diese Messkette zweimal an einem Vormittag den
Fehler gefunden, beide Male NICHT dort, wo gesucht wurde:

  * "kein ton" am SunSDR QRP — der Tonstrom lief mit vollen 12 kB/s, trug
    aber Stille (RMS -75 dBFS, 89 % der Proben null). Am Geraet hing keine
    Antenne. Die Software war tadellos.
  * "ton ein, immer noch nichts" — der Strom trug echten Ton (-37 dBFS),
    das Telefon gab ihn aus, und iOS verwarf ihn am Stummschalter.

Beide Male war die entscheidende Zahl nicht "laeuft es", sondern "wie laut
ist es". Darum misst dieses Werkzeug Pegel, nicht Zustaende.

Die Skripte dafuer lagen bis heute im Zwischenspeicher und waren nach dem
naechsten Neustart weg. Darum liegen sie jetzt hier.

Aufruf:

    python3 pruefe.py                      # alles, gegen die eigene Adresse
    python3 pruefe.py --host 172.30.30.121 --token RKQN-7DJN
    python3 pruefe.py ton                  # nur der Tonpegel
    python3 pruefe.py hf                   # nur Rauschboden und Abstand
    python3 pruefe.py pegel                # nur die Messwerte des Servers

Das Token braucht es nur fuer Verbindungen AUS DEM NETZ; von Loopback
verlangt Longpath keines (TciServer.cpp, istEigeneHerkunft).
"""

import argparse
import base64
import collections
import math
import os
import socket
import struct
import sys
import time

HDR = 64                       # TciBinaryFrame: 64-Byte-Kopf, 16x uint32 LE
STREAM_RX_TON = 1
STREAM_SPEKTRUM = 100          # Longpath-eigen
TYP_MULAW = 101
MULAW_VOLL = 32124.0           # groesster Betrag, den mu-law darstellt


# ── Draht ────────────────────────────────────────────────────────────────────

class Verbindung:
    """Ein TCI-Client, der nur zuhoert und misst."""

    def __init__(self, host, port, token, herkunft):
        self.sock = socket.create_connection((host, port), timeout=8)
        schluessel = base64.b64encode(os.urandom(16)).decode()
        self.sock.sendall((
            f"GET / HTTP/1.1\r\n"
            f"Host: {host}:{port}\r\n"
            f"Upgrade: websocket\r\n"
            f"Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {schluessel}\r\n"
            f"Sec-WebSocket-Version: 13\r\n"
            f"Origin: {herkunft}\r\n\r\n").encode())
        self.sock.settimeout(8)
        roh = b""
        while b"\r\n\r\n" not in roh:
            stueck = self.sock.recv(4096)
            if not stueck:
                raise RuntimeError("Verbindung vor dem Handschlag zu")
            roh += stueck
        kopf = roh.split(b"\r\n", 1)[0].decode(errors="replace")
        if "101" not in kopf:
            # 403 heisst: die Herkunftspruefung hat abgelehnt. Das ist eine
            # Aussage, keine Stoerung — darum im Klartext weitergeben.
            raise RuntimeError(f"Kein Upgrade — {kopf}")
        self.puffer = roh.split(b"\r\n\r\n", 1)[1]
        self.texte = []
        self.binaer = []
        if token:
            self.sende(f"auth:{token};")

    def sende(self, befehl):
        nutz = befehl.encode()
        maske = os.urandom(4)
        self.sock.sendall(bytes([0x81, 0x80 | len(nutz)]) + maske
                          + bytes(b ^ maske[i % 4] for i, b in enumerate(nutz)))

    def _entpacke(self):
        while len(self.puffer) >= 2:
            b1, b2 = self.puffer[0], self.puffer[1]
            laenge = b2 & 0x7F
            i = 2
            if laenge == 126:
                if len(self.puffer) < 4:
                    return
                laenge = int.from_bytes(self.puffer[2:4], "big")
                i = 4
            elif laenge == 127:
                if len(self.puffer) < 10:
                    return
                laenge = int.from_bytes(self.puffer[2:10], "big")
                i = 10
            if len(self.puffer) < i + laenge:
                return
            nutz = self.puffer[i:i + laenge]
            self.puffer = self.puffer[i + laenge:]
            opcode = b1 & 0x0F
            if opcode == 1:
                self.texte.append(nutz.decode(errors="replace"))
            elif opcode == 2:
                self.binaer.append(nutz)

    def sammle(self, sekunden, bis_ready=False):
        ende = time.time() + sekunden
        while time.time() < ende:
            try:
                stueck = self.sock.recv(262144)
            except socket.timeout:
                break
            if not stueck:
                break
            self.puffer += stueck
            self._entpacke()
            if bis_ready and any(z.strip() == "ready"
                                 for z in "".join(self.texte).split(";")):
                return True
        return not bis_ready

    def zeilen(self):
        return [z for z in "".join(self.texte).split(";") if z.strip()]

    def leeren(self):
        self.texte.clear()
        self.binaer.clear()

    def zu(self):
        try:
            self.sock.close()
        except OSError:
            pass


def rahmen_nach_art(binaer):
    nach = collections.defaultdict(list)
    for r in binaer:
        if len(r) >= HDR:
            nach[struct.unpack_from("<I", r, 24)[0]].append(r)
    return nach


# ── mu-law ───────────────────────────────────────────────────────────────────

def mulaw_wert(b):
    b = ~b & 0xFF
    vorzeichen = b & 0x80
    exponent = (b >> 4) & 7
    mantisse = b & 0x0F
    wert = (((mantisse << 1) + 33) << exponent) - 33
    return -wert if vorzeichen else wert


def db(wert, voll):
    return 20 * math.log10(max(abs(wert), 1e-9) / voll)


# ── Messungen ────────────────────────────────────────────────────────────────

def miss_ton(v, sekunden):
    """Kommt Ton an — und ist etwas drin?

    Die zweite Haelfte ist der Punkt. Ein voller Strom sagt nichts: 12 kB/s
    mu-law sind 12 kB/s, ob Sprache drinsteckt oder Stille.
    """
    for befehl in ("audio_samplerate:12000;", "audio_stream_channels:1;",
                   "audio_stream_sample_type:mulaw8;", "audio_start:0;"):
        v.sende(befehl)
        time.sleep(0.05)
    v.leeren()
    v.sammle(sekunden)
    rahmen = rahmen_nach_art(v.binaer).get(STREAM_RX_TON, [])
    kb = sum(len(r) for r in rahmen) / 1024.0
    print(f"  Tonrahmen          {len(rahmen):5d}  ({kb:.0f} kB in {sekunden} s)")
    if not rahmen:
        print("  -> Es kommt KEIN Ton. audio_start abgelehnt, oder kein Geraet.")
        return
    proben = [mulaw_wert(x) for r in rahmen for x in r[HDR:]]
    if not proben:
        print("  -> Rahmen ohne Inhalt.")
        return
    rms = math.sqrt(sum(p * p for p in proben) / len(proben))
    spitze = max(abs(p) for p in proben)
    still = sum(1 for p in proben if abs(p) <= 8) / len(proben)
    print(f"  Pegel RMS          {db(rms, MULAW_VOLL):6.1f} dBFS")
    print(f"  Pegel Spitze       {db(spitze, MULAW_VOLL):6.1f} dBFS")
    print(f"  praktisch still    {100 * still:6.1f} %")
    if still > 0.5:
        print("  -> Der Strom laeuft, traegt aber STILLE. Nicht die Software:")
        print("     Antenne, Vorverstaerker oder Stummschaltung pruefen.")
    else:
        print("  -> Es ist Ton drin. Hoert man ihn trotzdem nicht, liegt es")
        print("     am Geraet (iOS-Stummschalter, Lautstaerke, Bluetooth).")


def miss_hf(v, sekunden):
    """Steckt ueberhaupt ein Signal im Bild?

    Der Abstand zwischen Rauschboden und staerkstem Punkt trennt die Faelle
    sauber: ein belegtes Band bringt Traeger 30 bis 50 dB ueber den Boden,
    ein offener Eingang kaum 10. Am 2026-10-03 am selben Vormittag gemessen:
    7 dB am QRP ohne Antenne, 39 dB an der Anvelina mit.
    """
    v.sende("spectrum_start:0,373,10;")
    v.leeren()
    v.sammle(sekunden)
    rahmen = rahmen_nach_art(v.binaer).get(STREAM_SPEKTRUM, [])
    print(f"  Spektrumrahmen     {len(rahmen):5d}  in {sekunden} s")
    if not rahmen:
        print("  -> Kein Spektrum. Kennt der Server spectrum_start? Geraet dran?")
        return
    # dBm + 200, ein Byte je Bildpunkt (TciServer.cpp, sampleType 100).
    werte = sorted(b - 200 for b in rahmen[-1][HDR:])
    boden = werte[len(werte) // 5]          # 20er-Perzentil = Rauschen
    spitze = werte[-1]
    print(f"  Rauschboden (20 %) {boden:6.1f} dBm")
    print(f"  staerkster Punkt   {spitze:6.1f} dBm")
    print(f"  Abstand            {spitze - boden:6.1f} dB")
    if spitze - boden < 10:
        print("  -> KEIN SIGNAL. Das ist kein Bandzustand, das ist ein")
        print("     offener Eingang: Antenne, Stecker, Relais pruefen.")
    else:
        print("  -> Da sind Stationen.")


def miss_pegel(v, sekunden):
    """Was der Server selbst meldet — die Gegenprobe zu allem oben."""
    v.sende("rx_sensors_enable:true,200;")
    v.leeren()
    v.sammle(sekunden)
    gefunden = False
    for name in ("rx_sensors", "rx_channel_sensors_ex", "vfo:0,0",
                 "modulation:0", "dds:0"):
        treffer = [z for z in v.zeilen() if z.lower().startswith(name.lower())]
        if treffer:
            print(f"  {name:22s} {treffer[-1]}")
            gefunden = True
    if not gefunden:
        print("  -> Der Server meldet nichts. Ohne Token aus dem Netz? Dann")
        print("     schweigt er, bis auth: kommt.")


# ── Hauptteil ────────────────────────────────────────────────────────────────

def eigene_adresse():
    """Die Adresse, unter der dieser Rechner im Heimnetz steht."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("10.255.255.255", 1))     # schickt nichts, waehlt nur die Route
        return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        s.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("was", nargs="?", default="alles",
                    choices=["alles", "ton", "hf", "pegel"])
    ap.add_argument("--host", default=None, help="Vorgabe: die eigene Netzadresse")
    ap.add_argument("--port", type=int, default=50001)
    ap.add_argument("--token", default=None, help="nur aus dem Netz noetig")
    ap.add_argument("--dauer", type=float, default=5.0, help="Messdauer je Teil")
    args = ap.parse_args()

    host = args.host or eigene_adresse()
    herkunft = f"http://{host}:8771"
    print(f"Longpath auf {host}:{args.port}, Herkunft {herkunft}")

    try:
        v = Verbindung(host, args.port, args.token, herkunft)
    except (OSError, RuntimeError) as e:
        print(f"\nKeine Verbindung: {e}")
        print("Horcht der TCI-Server? Setup -> CAT & Network -> TCI Server.")
        return 1

    if not v.sammle(8, bis_ready=True):
        print("\nKein ready; — der Server schweigt.")
        print("Aus dem Netz verlangt Longpath ein Token (--token).")
        v.zu()
        return 1

    vfo = [z for z in v.zeilen() if z.startswith("vfo:0,0")]
    print(f"Verbunden. {vfo[-1] if vfo else 'keine VFO-Meldung'}\n")

    teile = ["hf", "pegel", "ton"] if args.was == "alles" else [args.was]
    for teil in teile:
        print({"ton": "── Ton ──", "hf": "── Signal im Bild ──",
               "pegel": "── Was der Server meldet ──"}[teil])
        {"ton": miss_ton, "hf": miss_hf, "pegel": miss_pegel}[teil](v, args.dauer)
        print()

    v.zu()
    return 0


if __name__ == "__main__":
    sys.exit(main())
