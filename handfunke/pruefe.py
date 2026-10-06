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
    python3 pruefe.py skala                # Skala gegen Preamp/ATT (40 s)

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
    # Ausdruecklich eine SCHMALE Spanne verlangen. Damit zeigt sich zugleich,
    # ob der Server die wirklich gezeigte Breite zurueckmeldet (`spectrum_span`,
    # eingefuehrt 2026-10-03): er hebt einen zu schmalen Wunsch naemlich an,
    # wenn die Punktzahl mehr Bins verlangt, als der Ausschnitt hergibt. Wer
    # mit seinem Wunsch weiterrechnet, setzt Abstimmstrich und Durchlassband
    # daneben.
    v.sende("spectrum_start:0,373,10,12000;")
    v.leeren()
    v.sammle(sekunden)
    rahmen = rahmen_nach_art(v.binaer).get(STREAM_SPEKTRUM, [])
    print(f"  Spektrumrahmen     {len(rahmen):5d}  in {sekunden} s")
    gemeldet = [z for z in v.zeilen() if z.lower().startswith("spectrum_span")]
    if gemeldet:
        try:
            hz = int(gemeldet[-1].rsplit(",", 1)[1])
            if hz == 0:
                print("  gezeigte Spanne      volle Breite (Abtastrate unbekannt)")
            else:
                print(f"  gezeigte Spanne    {hz:7d} Hz  (gewuenscht waren 12000)")
                if abs(hz - 12000) > 500:
                    print("     -> der Server hat den Wunsch angehoben; genau dafuer")
                    print("        gibt es die Meldung.")
        except (IndexError, ValueError):
            print(f"  gezeigte Spanne    {gemeldet[-1]}")
    else:
        print("  gezeigte Spanne      nicht gemeldet — alte Longpath-Fassung")
        print("                       oder fremder Server (dann gilt der Wunsch)")
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
    # Grenze aus Messungen vom 2026-10-03, nicht geschaetzt:
    #   SunSDR2 QRP ohne Antenne   7 / 8 / 10 dB
    #   ANVELINA PRO 3 mit Antenne 28 / 36 / 39 dB
    # Dazwischen liegt eine breite Luecke. Die erste Fassung stand bei 10 dB
    # und meldete bei exakt 10,0 "Da sind Stationen" — am QRP ohne Antenne,
    # bei einem zu 100 % stillen Tonstrom. Eine harte Kante an der falschen
    # Stelle ist schlimmer als keine; darum 15 dB und ein Zweifelsbereich,
    # der sich nicht festlegt, statt sich zu irren.
    abstand = spitze - boden
    if abstand < 15:
        print("  -> KEIN SIGNAL. Das ist kein Bandzustand, das ist ein")
        print("     offener Eingang: Antenne, Stecker, Relais pruefen.")
    elif abstand < 20:
        print("  -> ZWEIFELHAFT. Mehr als blosses Rauschen, aber weit unter")
        print("     dem, was ein belegtes Band bringt (30 bis 50 dB).")
    else:
        print("  -> Da sind Stationen.")


# Der Rueckfallwert des TCI-Servers, wenn es gar keinen Empfangskanal gibt
# (TciServer.cpp, rxSensorTimer: "Falls back to -140 dBm (WDSP noise floor
# convention) if WDSP isn't initialized or the channel doesn't exist yet").
#
# Er sieht aus wie eine Messung und ist keine. Am 2026-10-03 stand er
# waehrend der ganzen Fehlersuche im Bild, waehrend gar kein Funkgeraet
# verbunden war — und liess sich kaum von einem wirklich leeren Band
# unterscheiden. Das Werkzeug sagt es jetzt dazu.
KEIN_KANAL_DBM = -140.0


def miss_pegel(v, sekunden):
    """Was der Server selbst meldet — die Gegenprobe zu allem oben."""
    v.sende("rx_sensors_enable:true,200;")
    # Die Frequenz ausdruecklich ABFRAGEN. Sie kommt sonst nur im Init-Burst,
    # und der ist hier laengst verbraucht — die Pruefung darauf liefe leer.
    v.sende("vfo:0,0;")
    v.leeren()
    v.sammle(sekunden)
    gefunden = False
    letzte = {}
    for name in ("rx_sensors", "rx_channel_sensors_ex", "vfo:0,0",
                 "modulation:0", "dds:0"):
        treffer = [z for z in v.zeilen() if z.lower().startswith(name.lower())]
        if treffer:
            print(f"  {name:22s} {treffer[-1]}")
            letzte[name] = treffer[-1]
            gefunden = True
    if not gefunden:
        print("  -> Der Server meldet nichts. Ohne Token aus dem Netz? Dann")
        print("     schweigt er, bis auth: kommt.")
        return

    # Zwei Anzeichen, dass ueberhaupt kein Geraet dranhaengt — und beide
    # sehen harmlos aus, wenn man sie einzeln liest.
    hinweise = []
    vfo = letzte.get("vfo:0,0", "")
    if vfo.endswith(",0"):
        hinweise.append("die abgestimmte Frequenz ist 0")
    sens = letzte.get("rx_sensors", "")
    try:
        wert = float(sens.rsplit(",", 1)[1])
        if abs(wert - KEIN_KANAL_DBM) < 0.05:
            hinweise.append(f"{KEIN_KANAL_DBM:.0f} dBm ist der Rueckfallwert "
                            "fuer 'kein Empfangskanal', keine Messung")
    except (IndexError, ValueError):
        pass
    if hinweise:
        print("  -> KEIN FUNKGERAET verbunden: " + "; ".join(hinweise) + ".")
        print("     Alles Weitere misst dann nur die leere Kette.")


# ── Hauptteil ────────────────────────────────────────────────────────────────

def miss_skala(v, sekunden):
    """Bleibt die Skala stehen, wenn sich die Vorverstaerkung aendert?

    Fuer den Live-Test, der an PR #102 haengt: Kalibrierung auf die Daten,
    Achse fest. Der Betreiber schaltet waehrend des Laufs Preamp/ATT durch
    (+10 / 0 / -10 / -20 dB); richtig ist es, wenn Rauschboden und S-Meter
    STEHENBLEIBEN -- die Kalibrierung gleicht die Stufe ja gerade aus.

    Bisher war das ein Augenmass-Test ("sieht gleich aus"). Hier sind es
    Zahlen: springt der Boden um rund die Stufenhoehe, wirkt die
    Kalibrierung nicht; bleibt er innerhalb von ein, zwei dB, wirkt sie.

    Ein Hinweis zur Lesart, der mich selbst fast hereingelegt haette: ohne
    Antenne ist der Rauschboden der EIGENE Empfaengerrauschanteil, und der
    aendert sich mit der Vorverstaerkung wirklich. Der Test braucht ein
    stehendes Signal -- Traeger oder Bake --, und dann ist die SPITZE die
    Zahl, auf die es ankommt.
    """
    v.sende("rx_sensors_enable:true,200;")
    v.sende("spectrum_start:0,373,10,12000;")
    v.leeren()

    print(f"  Jede Sekunde eine Zeile, {sekunden} s lang.")
    print("  Jetzt Preamp/ATT durchschalten: +10 / 0 / -10 / -20 dB.")
    print()
    print("     t   S-Meter      Boden     Spitze    Abstand")
    print("   ---  --------   --------   --------   --------")

    smeter, boeden, spitzen = [], [], []
    for t in range(1, int(sekunden) + 1):
        v.sammle(1.0)
        rahmen = rahmen_nach_art(v.binaer).get(STREAM_SPEKTRUM, [])
        sm = None
        for z in v.zeilen():
            if z.lower().startswith("rx_sensors"):
                try:
                    sm = float(z.rstrip(";").rsplit(",", 1)[1])
                except (IndexError, ValueError):
                    pass
        boden = spitze = None
        if rahmen:
            werte = sorted(b - 200 for b in rahmen[-1][HDR:])
            boden, spitze = werte[len(werte) // 5], werte[-1]

        def z(x):
            return "    —   " if x is None else f"{x:8.1f}"

        abst = "    —   " if (boden is None or spitze is None) \
               else f"{spitze - boden:8.1f}"
        print(f"   {t:3d}  {z(sm)}   {z(boden)}   {z(spitze)}   {abst}")

        # Der Rueckfallwert zaehlt nicht als Messung (siehe KEIN_KANAL_DBM).
        if sm is not None and abs(sm - KEIN_KANAL_DBM) > 0.5:
            smeter.append(sm)
        if boden is not None:
            boeden.append(boden)
        if spitze is not None:
            spitzen.append(spitze)
        v.leeren()

    print()

    def spanne(name, werte, grenze):
        if len(werte) < 2:
            print(f"  {name:12s} zu wenige Messwerte")
            return
        d = max(werte) - min(werte)
        print(f"  {name:12s} {min(werte):7.1f} bis {max(werte):7.1f} dBm"
              f"   -> Spanne {d:5.1f} dB")
        return d

    sS = spanne("S-Meter", smeter, 2.0)
    spanne("Rauschboden", boeden, 2.0)
    sP = spanne("Spitze", spitzen, 2.0)

    print()
    if sP is None:
        print("  -> Kein Spektrum bekommen; ohne Bild sagt der Lauf nichts.")
        return
    if sP <= 2.0:
        print("  -> Die SPITZE blieb innerhalb von 2 dB stehen. Genau so soll")
        print("     es sein: die Kalibrierung gleicht die Stufe aus.")
    elif sP < 6.0:
        print("  -> Die Spitze wanderte um {:.1f} dB. Das ist mehr als".format(sP))
        print("     Messrauschen und weniger als eine ganze Stufe — zweiter")
        print("     Lauf mit einem STEHENDEN Traeger, bevor man das deutet.")
    else:
        print("  -> Die Spitze wanderte um {:.1f} dB. Das ist die".format(sP))
        print("     Groessenordnung der Preamp-Stufen selbst: die Kalibrierung")
        print("     wirkt nicht auf die Ablesung.")
    if sS is not None and sP is not None and abs(sS - sP) > 6.0:
        print()
        print("  -> S-Meter und Spektrum bewegen sich UNTERSCHIEDLICH")
        print(f"     ({sS:.1f} dB gegen {sP:.1f} dB).")
        print("     Das IST der Befund hinter #102 -- aber nur, wenn das")
        print("     Signal selbst stillstand. Schwankt der Traeger (oder ist")
        print("     es eine Attrappe mit wanderndem S-Meter), sagt der")
        print("     Unterschied nichts. Erst mit stehendem Traeger deuten.")


def miss_logbuch(v, sekunden):
    """Was die App im Logbuch sieht — und wie lange Longpath dafuer braucht.

    LIEST NUR. `log_last:` und `log_dup:` haengen nichts an und aendern
    nichts; der Schreibbefehl `log_qso:` wird hier bewusst nicht benutzt,
    damit dieses Werkzeug an Martins echtem Logbuch laufen kann.

    Gemessen wird die ANTWORTZEIT, nicht nur das Zustandekommen. Die
    Entwurfsentscheidung "kein zweiter Index, nur ein Durchlauf" steht und
    faellt damit: bei 6,6 MB kostet ein Durchlauf gemessene 3,8 ms, und
    wenn daraus im Betrieb einmal 300 ms werden, gehoert das gesehen und
    nicht geraten.
    """
    v.leeren()
    t0 = time.time()
    v.sende("log_last:10;")
    v.sammle(min(sekunden, 3.0))
    ms = (time.time() - t0) * 1000.0
    zeilen = [z for z in v.zeilen() if z.startswith("log_qso_zeile:")]
    ende = [z for z in v.zeilen() if z.startswith("log_last_")]

    if not ende:
        print("  log_last: keine Abschlusszeile — der Server kennt den "
              "Befehl nicht (vor 2026-10-04) oder schweigt.")
        return
    if ende[-1].startswith("log_last_err:"):
        print(f"  log_last abgelehnt: {ende[-1].split(':', 1)[1]}")
        print("  Aus dem Netz verlangt Longpath TciAllowRemoteLog.")
        return

    angesagt = ende[-1].split(":", 1)[1]
    print(f"  {len(zeilen)} Zeilen, angesagt {angesagt}, in {ms:.0f} ms")
    # Die Zahl MUSS stimmen. Eine kuerzere Liste sieht aus wie ein
    # kuerzeres Logbuch, und genau das darf nicht unbemerkt bleiben.
    if angesagt.strip() != str(len(zeilen)):
        print("  ACHTUNG: Anzahl und Zeilen stimmen nicht ueberein.")
    for z in zeilen[:5]:
        f = z.split(":", 1)[1].split(",")
        while len(f) < 8:
            f.append("")
        print(f"    {f[1]} {f[2]}  {f[3]:<10} {f[4]:>4} {f[5]:<5} "
              f"{f[6]}/{f[7]}")

    # Und die Dupe-Frage, mit dem jüngsten Rufzeichen aus der Liste: eines,
    # das garantiert im Logbuch steht. Ein erfundenes Rufzeichen wuerde nur
    # belegen, dass "nie gearbeitet" funktioniert.
    if not zeilen:
        return
    ruf = zeilen[0].split(":", 1)[1].split(",")[3].strip()
    if not ruf:
        return
    v.leeren()
    t0 = time.time()
    v.sende(f"log_dup:{ruf};")
    v.sammle(min(sekunden, 3.0))
    ms = (time.time() - t0) * 1000.0
    antwort = [z for z in v.zeilen() if z.startswith("log_dup_")]
    if not antwort:
        print(f"  log_dup:{ruf} — keine Antwort")
        return
    print(f"  log_dup:{ruf} in {ms:.0f} ms -> {antwort[-1]}")
    if antwort[-1].startswith("log_dup_ok:"):
        f = antwort[-1].split(":", 1)[1].split(",")
        if len(f) >= 2 and f[1].strip() in ("", "0"):
            print("  ACHTUNG: ein Rufzeichen AUS der Liste gilt als nie "
                  "gearbeitet — das Vorsieb findet seinen eigenen Eintrag "
                  "nicht.")


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
                    choices=["alles", "ton", "hf", "pegel", "skala",
                             "logbuch"])
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

    teile = (["hf", "pegel", "ton", "logbuch"] if args.was == "alles"
             else [args.was])
    for teil in teile:
        print({"ton": "── Ton ──", "hf": "── Signal im Bild ──",
               "pegel": "── Was der Server meldet ──",
               "skala": "── Skala gegen Preamp/ATT ──",
               "logbuch": "── Logbuch, wie die App es sieht ──"}[teil])
        # Der Skala-Lauf braucht Zeit zum Durchschalten, nicht fuenf Sekunden.
        dauer = max(args.dauer, 40.0) if teil == "skala" else args.dauer
        {"ton": miss_ton, "hf": miss_hf, "pegel": miss_pegel,
         "skala": miss_skala, "logbuch": miss_logbuch}[teil](v, dauer)
        print()

    v.zu()
    return 0


if __name__ == "__main__":
    sys.exit(main())
