#!/usr/bin/env python3
"""attrappe.py — ein TCI-Server aus Pappe, nur zum Pruefen der Handfunke.

Er ersetzt KEIN Longpath. Er tut genau so viel, wie noetig ist, damit die
Handfunke ohne Funkgeraet zeigen kann, was sie kann: Init-Burst, ein
Spektrum mit erkennbaren Traegern, Empfangston.

Ohne fremde Bibliotheken — der WebSocket-Handschlag und die Rahmen sind
hier zu Fuss gebaut (RFC 6455), weil python3 hier kein 'websockets' hat.

Der Binaerrahmen ist derselbe wie in TciBinaryFrame.h:
  64-Byte-Kopf, 16 x uint32 little-endian
  Offset 0 receiver · 4 sampleRate · 8 sampleType · 20 length · 24 streamType
  sampleType 3 = Float32 · streamType 0 = IQ, 1 = RX-Ton
"""

import base64, hashlib, math, os, random, socket, struct, threading, time, sys

# --port N: fuer den Fall, dass schon eine Attrappe laeuft. Am 2026-10-03
# hing eine seit elf Stunden auf 50099 -- sie abzuschiessen waere die
# bequeme und die falsche Antwort gewesen (sie koennte zu einer anderen
# Sitzung gehoeren).
PORT = 50099
if '--port' in sys.argv:
    _i = sys.argv.index('--port')
    if _i + 1 < len(sys.argv):
        PORT = int(sys.argv[_i + 1])
# Ein Band ohne Stationen — siehe die Begruendung bei `traeger`.
STILL = '--still' in sys.argv
# --sendet N: alle N Sekunden den Sendezustand umschalten (trx:0,true/false).
#
# Warum als Schalter der Attrappe und nicht am echten Geraet geprueft: den
# Sendezustand am echten Geraet herzustellen heisst SENDEN, und das ist ohne
# Antenne verboten. Die Seite muss aber zeigen koennen, dass die Station
# sendet -- sonst ist der leere Wasserfall waehrend einer Durchsage nicht von
# einem Fehler zu unterscheiden. Also hier.
def _zahl_nach(flagge, standard):
    if flagge in sys.argv:
        i = sys.argv.index(flagge)
        if i + 1 < len(sys.argv):
            try:
                return float(sys.argv[i + 1])
            except ValueError:
                pass
        return standard
    return 0.0

SENDET_ALLE = _zahl_nach('--sendet', 6.0)
# Ab Werk gesperrt, wie TciAllowRemoteTx. --sendenfrei gibt frei.
SENDEN_FREI = '--sendenfrei' in sys.argv
IQ_RATE = 48000          # bewusst klein: die Attrappe soll die Naht pruefen,
AUDIO_RATE = 48000       # nicht die Bandbreite
IQ_BLOCK = 4096          # Werte je Rahmen (I und Q zusammen) -> 2048 Paare
AUDIO_BLOCK = 2048       # Werte je Rahmen (L und R zusammen) -> 1024 Paare

MAGIC = b'258EAFA5-E914-47DA-95CA-C5AB0DC85B11'


def _jetzt_utc():
    """("20261004", "213000") — so wie Longpath es in log_qso_zeile schickt."""
    t = time.gmtime()
    return (time.strftime('%Y%m%d', t), time.strftime('%H%M%S', t))


# Ein kleiner Vorrat, damit die Liste nicht leer anfaengt, und damit sich
# ALLE vier Faelle des Dupe-Satzes am Telefon ansehen lassen:
#   OE1AAA  20m CW  -> echtes Dupe (Attrappe steht auf 20m CW)
#   OE2BBB  20m SSB -> gleiches Band, andere Betriebsart
#   OE3CCC  40m SSB -> bekannt, dieses Band noch nicht
#   alles andere    -> NEU
VORRAT = [
    (('20261001', '081500'), 'OE3CCC', '40m', 'SSB', '59', '59'),
    (('20261002', '143000'), 'OE2BBB', '20m', 'SSB', '59', '57'),
    (('20261002', '191200'), 'OE1AAA', '20m', 'CW',  '599', '599'),
]
EINGETRAGEN = []

INIT_BURST = [
    'protocol:Longpath-Attrappe,2.0;',
    'device:Attrappe;',
    'receive_only:false;',
    'trx_count:2;',
    'channels_count:2;',
    'vfo_limits:1800000,54000000;',
    'if_limits:-48000,48000;',
    'modulations_list:am,sam,dsb,lsb,usb,cw,nfm,digl,digu,drm;',
    f'iq_samplerate:{IQ_RATE};',
    f'audio_samplerate:{AUDIO_RATE};',
    'vfo:0,0,14074000;',
    'vfo:0,1,14074000;',
    'vfo:1,0,7100000;',
    'vfo:1,1,7100000;',
    # Die Mitte des Bildes (die DDC-Frequenz), NICHT die abgestimmte.
    #
    # Fehlte bis zum 2026-10-03, und die Luecke war kein Schoenheitsfehler:
    # die Handfunke rechnet Abstimmstrich UND Durchlassband aus dem Abstand
    # zwischen abgestimmter Frequenz und Bildmitte. Ohne `dds:` kennt sie die
    # Mitte nicht, zeichnet darum gar kein Band (richtig so -- lieber nichts
    # als etwas an der falschen Stelle) und setzt den Strich stur auf 50 %.
    # An der Werkbank sah damit beides kaputt aus, was am echten Geraet
    # einwandfrei laeuft.
    'dds:0,14074000;',
    'dds:1,7100000;',
    'modulation:0,usb;',
    'modulation:1,lsb;',
    'rx_filter_band:0,100,2500;',
    'rx_filter_band:1,-2500,-100;',
    'drive:0,55;',
    'drive:1,55;',
    'tune_drive:0,10;',
    'tune_drive:1,10;',
    'trx:0,false;',
    'trx:1,false;',
    'tune:0,false;',
    'tune:1,false;',
    'volume:-12.0;',
    'mute:false;',
    'start;',
    'ready;',
]


def frame(payload, opcode):
    """Ein Rahmen vom Server: nie maskiert (RFC 6455 §5.1)."""
    n = len(payload)
    head = bytes([0x80 | opcode])
    if n < 126:
        head += bytes([n])
    elif n < (1 << 16):
        head += bytes([126]) + struct.pack('>H', n)
    else:
        head += bytes([127]) + struct.pack('>Q', n)
    return head + payload


def spectrum_frame(receiver, fps, dbm_werte):
    """Der Longpath-eigene Spektrumrahmen: ein Byte je Wert, dBm + 200."""
    hdr = [0] * 16
    hdr[0] = receiver
    hdr[1] = fps                # hier steht die Bildrate, nicht die Abtastrate
    hdr[2] = 100                # UInt8Dbm
    hdr[5] = len(dbm_werte)
    hdr[6] = 100                # SpectrumStream
    hdr[7] = 1
    bytes_ = bytes(max(0, min(255, int(round(d + 200)))) for d in dbm_werte)
    return b''.join(struct.pack('<I', v) for v in hdr) + bytes_


def stream_frame(receiver, sample_rate, stream_type, values,
                 sample_type=3, channels=2):
    """64-Byte-Kopf + Nutzlast, genau wie TciBinaryFrame.

    sample_type 3 = Float32 (vier Byte), 0 = Int16 (zwei Byte).
    """
    hdr = [0] * 16
    hdr[0] = receiver
    hdr[1] = sample_rate
    hdr[2] = sample_type
    hdr[5] = len(values)       # ALLE Werte, nicht je Kanal
    hdr[6] = stream_type
    hdr[7] = channels
    kopf = b''.join(struct.pack('<I', v) for v in hdr)
    if sample_type == 0:
        nutz = struct.pack(f'<{len(values)}h',
                           *(max(-32768, min(32767, int(round(v * 32767))))
                             for v in values))
    else:
        nutz = struct.pack(f'<{len(values)}f', *values)
    return kopf + nutz


class Verbindung(threading.Thread):
    def __init__(self, sock, addr):
        super().__init__(daemon=True)
        self.sock, self.addr = sock, addr
        self.lauft = True
        self.iq_an = False
        self.audio_an = False
        self.spec_an = False
        self.spec_punkte = 256
        self.spec_fps = 10
        # Messwerte kommen erst auf Anforderung — so wie beim echten Server
        # (TciClientSession::rxSensorsEnabled steht ab Werk auf false).
        self.rx_sensors_an = False
        self.rx_sensors_ms = 200
        self.spec_spanne = 0          # 0 = volle Breite
        self.spec_zeit = 0.0
        # Vom Client ausgehandelt (audio_samplerate / _channels / _sample_type).
        self.audio_rate = AUDIO_RATE
        self.audio_kanaele = 2
        self.audio_typ = 3        # Float32
        self.phase = 0.0
        self.tonphase = 0.0
        # Letzter gemeldeter Sendezustand (nur fuer --sendet).
        self.sendet = False

    # ── Handschlag ────────────────────────────────────────────────────────
    def handschlag(self):
        daten = b''
        while b'\r\n\r\n' not in daten:
            teil = self.sock.recv(4096)
            if not teil:
                return False
            daten += teil
        kopf = {}
        for zeile in daten.split(b'\r\n')[1:]:
            if b':' in zeile:
                k, _, v = zeile.partition(b':')
                kopf[k.strip().lower()] = v.strip()
        key = kopf.get(b'sec-websocket-key')
        if not key:
            return False
        akzept = base64.b64encode(hashlib.sha1(key + MAGIC).digest())
        self.sock.sendall(
            b'HTTP/1.1 101 Switching Protocols\r\n'
            b'Upgrade: websocket\r\n'
            b'Connection: Upgrade\r\n'
            b'Sec-WebSocket-Accept: ' + akzept + b'\r\n\r\n')
        return True

    def sende_text(self, s):
        try:
            self.sock.sendall(frame(s.encode('utf-8'), 0x1))
        except OSError:
            self.lauft = False

    def sende_binaer(self, b):
        try:
            self.sock.sendall(frame(b, 0x2))
        except OSError:
            self.lauft = False

    # ── Eingehende Befehle ────────────────────────────────────────────────
    def lesen(self):
        while self.lauft:
            try:
                kopf = self._genau(2)
                if not kopf:
                    break
                opcode = kopf[0] & 0x0F
                maskiert = kopf[1] & 0x80
                n = kopf[1] & 0x7F
                if n == 126:
                    n = struct.unpack('>H', self._genau(2))[0]
                elif n == 127:
                    n = struct.unpack('>Q', self._genau(8))[0]
                maske = self._genau(4) if maskiert else b'\0\0\0\0'
                nutz = bytearray(self._genau(n) or b'')
                for i in range(len(nutz)):
                    nutz[i] ^= maske[i % 4]
                if opcode == 0x8:
                    break
                if opcode == 0x1:
                    for teil in bytes(nutz).decode('utf-8', 'replace').split(';'):
                        if teil.strip():
                            self.befehl(teil.strip())
            except OSError:
                break
        self.lauft = False

    def _genau(self, n):
        buf = b''
        while len(buf) < n:
            teil = self.sock.recv(n - len(buf))
            if not teil:
                return None
            buf += teil
        return buf

    def befehl(self, zeile):
        name, _, rest = zeile.partition(':')
        name = name.lower()
        args = [a.strip() for a in rest.split(',')] if rest else []
        if name == 'auth':
            # Die Attrappe prueft nichts — sie protokolliert nur, DASS und WANN
            # die Anmeldung kommt. Der echte Server prueft sie (siehe
            # tst_tci_remote_auth).
            print(f'  {self.addr[1]}: auth empfangen ({len(args[0]) if args else 0} Zeichen)')
            self.sende_text('auth:ok;')
        elif name == 'log_qso':
            # Longpath-eigener Befehl (PR #184). Die Attrappe schreibt nichts,
            # sie antwortet nur — damit sich das Blatt auf der Seite pruefen
            # laesst, ohne Martins Logbuch anzufassen.
            #
            # Sie ist dabei absichtlich NICHT gutmuetiger als der echte
            # Server: leeres oder zu langes Rufzeichen wird genauso
            # abgelehnt. Eine Attrappe, die mehr durchlaesst als das
            # Original, verschiebt Fehler nach hinten (siehe rx_sensors).
            ruf = (args[0].upper() if args else '')
            if not ruf or len(ruf) > 20:
                print(f'  {self.addr[1]}: log_qso ABGELEHNT ({ruf!r})')
                self.sende_text('log_qso_err:rufzeichen fehlt;')
            else:
                rs = args[1] if len(args) > 1 and args[1] else '59'
                re_ = args[2] if len(args) > 2 and args[2] else '59'
                print(f'  {self.addr[1]}: log_qso {ruf} {rs}/{re_}')
                self.sende_text(f'log_qso_ok:{ruf};')
                # Eingetragenes merken, damit `log_last` und `log_dup`
                # etwas zu sagen haben. Nur im Speicher dieser Attrappe —
                # Martins Logbuch wird hier nicht angefasst.
                EINGETRAGEN.append((_jetzt_utc(), ruf, '20m', 'CW', rs, re_))
        elif name == 'log_last':
            # Longpath-eigener Befehl (2026-10-04). Je Kontakt eine Zeile,
            # dann der Abschluss mit der Anzahl.
            #
            # Die Attrappe deckelt wie der echte Server auf 50 — eine
            # Attrappe, die mehr durchlaesst, verschiebt Fehler nach hinten.
            try:
                n = int(args[0]) if args and args[0].strip() else 10
            except ValueError:
                n = 10
            if n <= 0:
                n = 10
            n = min(n, 50)
            liste = (VORRAT + EINGETRAGEN)[::-1][:n]
            for i, (zeit, ruf, band, mode, rs, re_) in enumerate(liste):
                self.sende_text(
                    f'log_qso_zeile:{i},{zeit[0]},{zeit[1]},{ruf},'
                    f'{band},{mode},{rs},{re_};')
            print(f'  {self.addr[1]}: log_last {n} -> {len(liste)} Zeilen')
            self.sende_text(f'log_last_ok:{len(liste)};')
        elif name == 'log_dup':
            ruf = (args[0].strip().upper() if args else '')
            if not ruf or len(ruf) > 20:
                print(f'  {self.addr[1]}: log_dup ABGELEHNT ({ruf!r})')
                self.sende_text('log_dup_err:rufzeichen fehlt;')
            else:
                # Band und Betriebsart kommen beim echten Server aus der
                # aktiven Scheibe. Die Attrappe steht auf 20m CW.
                treffer = [e for e in (VORRAT + EINGETRAGEN) if e[1] == ruf]
                anzahl = len(treffer)
                letzter = treffer[-1] if treffer else None
                gleiches_band = any(e[2] == '20m' for e in treffer)
                dupe = any(e[2] == '20m' and e[3] == 'CW' for e in treffer)
                dat = letzter[0][0] if letzter else ''
                zei = letzter[0][1] if letzter else ''
                band = letzter[2] if letzter else ''
                mode = letzter[3] if letzter else ''
                print(f'  {self.addr[1]}: log_dup {ruf} -> {anzahl}× '
                      f'(Band {int(gleiches_band)}, Dupe {int(dupe)})')
                self.sende_text(
                    f'log_dup_ok:{ruf},{anzahl},{dat},{zei},{band},{mode},'
                    f'{int(gleiches_band)},{int(dupe)};')
        elif name in ('trx', 'tune'):
            # Sendesperre, wie der echte Server sie fuehrt (PR #189-Reihe).
            # Die Attrappe ist dabei absichtlich NICHT gutmuetiger: ohne
            # ausdrueckliche Freigabe wird abgelehnt, und zwar MIT Grund.
            #
            # Bis zum 2026-10-04 verwarf der Server solche Befehle stumm. Auf
            # einer Fernbedienung ist ein stummes Nein nicht von einem Defekt
            # zu unterscheiden -- der Bediener drueckt, nichts passiert,
            # nichts erklaert es. Genau dieser Fall muss sich hier pruefen
            # lassen, ohne dass irgendwo ein Watt entsteht.
            #
            # --sendenfrei schaltet die Freigabe ein; ohne das bleibt es
            # gesperrt, wie TciAllowRemoteTx ab Werk.
            will = len(args) >= 2 and args[1].strip().lower() == 'true'
            if will and not SENDEN_FREI:
                print(f'  {self.addr[1]}: {name} ABGELEHNT (nicht freigegeben)')
                self.sende_text('tx_err:nicht freigegeben;')
            else:
                nr = args[0].strip() if args else '0'
                print(f'  {self.addr[1]}: {name}:{nr},{"true" if will else "false"}')
                self.sende_text(f'{name}:{nr},{"true" if will else "false"};')
        elif name == 'iq_start':
            self.iq_an = True
            print(f'  {self.addr[1]}: IQ an')
        elif name == 'iq_stop':
            self.iq_an = False
        elif name == 'rx_sensors_enable':
            # Thetis-getreu: args[0] = true/false, args[1] optional das
            # Intervall in Millisekunden.
            self.rx_sensors_an = (args[0].strip().lower() == 'true') if args else False
            if len(args) >= 2:
                try: self.rx_sensors_ms = max(30, min(1000, int(args[1])))
                except ValueError: pass
        elif name == 'audio_start':
            self.audio_an = True
            print(f'  {self.addr[1]}: Ton an')
        elif name == 'audio_stop':
            self.audio_an = False
        elif name == 'spectrum_start':
            self.spec_an = True
            if len(args) >= 2 and args[1].isdigit():
                self.spec_punkte = max(64, min(1024, int(args[1])))
            if len(args) >= 3 and args[2].isdigit():
                self.spec_fps = max(1, min(30, int(args[2])))
            if len(args) >= 4 and args[3].isdigit():
                # Vierter Wert: gezeigte Bandbreite in Hertz, 0 = alles.
                # Longpath schneidet dafuer mittig aus den FFT-Bins; hier
                # wird das Bild einfach schmaler gerechnet. Fuer die
                # Bedienprobe ist das gleichwertig — und ohne das laesst
                # sich das Kneifen nicht ohne Funkgeraet ausprobieren.
                hz = int(args[3])
                self.spec_spanne = 0 if hz <= 0 else max(2000, min(IQ_RATE, hz))
            self.sende_text(
                f'spectrum_start:{args[0]},{self.spec_punkte},{self.spec_fps};')
            print(f'  {self.addr[1]}: Spektrum an, {self.spec_punkte} Punkte,'
                  f' {self.spec_fps} B/s')
        elif name == 'audio_samplerate':
            if args and args[0].isdigit():
                self.audio_rate = int(args[0])
                self.sende_text(f'audio_samplerate:{self.audio_rate};')
                print(f'  {self.addr[1]}: Tonrate {self.audio_rate}')
        elif name == 'audio_stream_channels':
            if args and args[0].isdigit():
                self.audio_kanaele = max(1, min(2, int(args[0])))
                self.sende_text(f'audio_stream_channels:{self.audio_kanaele};')
                print(f'  {self.addr[1]}: Tonkanaele {self.audio_kanaele}')
        elif name == 'audio_stream_sample_type':
            typen = {'int16': 0, 'int24': 1, 'int32': 2, 'float32': 3}
            if args and args[0].lower() in typen:
                self.audio_typ = typen[args[0].lower()]
                self.sende_text(f'audio_stream_sample_type:{args[0].lower()};')
                print(f'  {self.addr[1]}: Tontyp {args[0].lower()}')
        elif name == 'spectrum_stop':
            self.spec_an = False
            self.sende_text(f'spectrum_stop:{args[0]};')
        elif name in ('drive', 'tune_drive', 'vfo', 'modulation',
                      'rx_filter_band', 'trx', 'tune'):
            # Wie der echte Server: setzen und die Gegenmeldung schicken,
            # damit der Client seinen Zustand NUR vom Server bekommt.
            if len(args) >= 2:
                self.sende_text(f'{name}:{",".join(args)};')
            # Beim Abstimmen wandert die Bildmitte mit, solange die neue
            # Frequenz nicht mehr ins alte Fenster passt. Die Attrappe macht
            # es sich einfach und zieht die Mitte immer nach: so pruefen
            # Abstimmstrich und Durchlassband wenigstens den Normalfall
            # (Mitte = abgestimmt). Das feinere Verhalten -- Mitte steht,
            # Strich wandert, bis der Rand kommt -- kann nur das echte
            # Geraet zeigen.
            if name == 'vfo' and len(args) >= 3:
                try:
                    trx = int(args[0]); kanal = int(args[1]); hz = int(args[2])
                    if kanal == 0:
                        self.sende_text(f'dds:{trx},{hz};')
                except ValueError:
                    pass
            print(f'  {self.addr[1]}: {zeile}')

    # ── Stroeme ───────────────────────────────────────────────────────────
    def run(self):
        if not self.handschlag():
            self.sock.close()
            return
        print(f'Client {self.addr[0]}:{self.addr[1]} verbunden')
        for zeile in INIT_BURST:
            self.sende_text(zeile)
        threading.Thread(target=self.lesen, daemon=True).start()

        # Traeger, die die Handfunke zeigen soll: Versatz in Hz von der Mitte,
        # Staerke, und ob sie dauernd da sind oder blinken.
        # Die Traeger stehen auf FESTEN Frequenzen, nicht relativ zur Mitte.
        # Nur so bewirkt das Abstimmen etwas Sichtbares: faehrt man auf einen
        # zu, wandert er ins Bild. Lagen sie relativ zur Mitte, stuende das
        # Bild still — und genau das war am echten Geraet die Beschwerde.
        traeger = [(-14000, 0.22, 0.0), (-6200, 0.09, 0.0), (-1500, 0.45, 0.0),
                   (3100, 0.13, 0.7), (8800, 0.30, 0.0), (15500, 0.06, 1.3)]
        # --still: ein Band ohne jede Station.
        #
        # Klingt nach einem nutzlosen Modus und ist doch der wichtigste. Am
        # 2026-10-03 hat der Betreiber aus einem laufenden, bunten Wasserfall
        # geschlossen, es komme HF an — gemessen waren 7 dB zwischen
        # Rauschboden und staerkstem Punkt, am Geraet hing keine Antenne. Seit
        # der Wasserfall auf dem GEMESSENEN Rauschboden sitzt, malt er auch
        # reines Rauschen bunt; der Hinweis "nur rauschen — antenne?" in der
        # Fusszeile faengt das ab. Ohne diesen Modus liesse er sich an der
        # Werkbank nicht pruefen, denn die Attrappe hat immer Traeger.
        if STILL:
            traeger = []
        t0 = time.time()
        smeter_zeit = 0.0

        while self.lauft:
            jetzt = time.time() - t0

            if self.iq_an:
                werte = []
                for i in range(IQ_BLOCK // 2):
                    t = self.phase + i / IQ_RATE
                    re = random.gauss(0, 0.012)
                    im = random.gauss(0, 0.012)
                    for hz, amp, blink in traeger:
                        a = amp
                        if blink:
                            # langsam kommen und gehen, damit der Wasserfall
                            # etwas zu erzaehlen hat
                            a *= max(0.0, math.sin(jetzt / blink) ** 2)
                        w = 2 * math.pi * hz * t
                        re += a * math.cos(w)
                        im += a * math.sin(w)
                    werte.append(re)
                    werte.append(im)
                self.phase += (IQ_BLOCK // 2) / IQ_RATE
                self.sende_binaer(stream_frame(0, IQ_RATE, 0, werte))

            if self.audio_an:
                # Gleiche Spieldauer wie vorher, aber in der ausgehandelten
                # Rate: bei 12 kHz sind das ein Viertel der Abtastungen.
                rahmen = max(1, int((AUDIO_BLOCK // 2) * self.audio_rate / AUDIO_RATE))
                werte = []
                for i in range(rahmen):
                    t = self.tonphase + i / self.audio_rate
                    # Ein ruhiger Zweiklang, damit man hoert, dass es laeuft,
                    # ohne dass es nach Alarm klingt.
                    v = 0.09 * math.sin(2 * math.pi * 620 * t) \
                      + 0.05 * math.sin(2 * math.pi * 930 * t) \
                      + random.gauss(0, 0.004)
                    for _ in range(self.audio_kanaele):
                        werte.append(v)
                self.tonphase += rahmen / self.audio_rate
                self.sende_binaer(stream_frame(0, self.audio_rate, 1, werte,
                                               self.audio_typ, self.audio_kanaele))

            if self.spec_an and (jetzt - self.spec_zeit) >= 1.0 / self.spec_fps:
                self.spec_zeit = jetzt
                p = self.spec_punkte
                spanne = self.spec_spanne or IQ_RATE
                werte = []
                for i in range(p):
                    hz = -spanne / 2 + spanne * i / (p - 1)
                    v = -128.0 + random.gauss(0, 1.5)   # Grundrauschen in dBm
                    for thz, amp, blink in traeger:
                        a = amp
                        if blink:
                            a *= max(0.0, math.sin(jetzt / blink) ** 2)
                        if a <= 0:
                            continue
                        d = hz - thz
                        # Traegerbreite rund 300 Hz, in dB ueber dem Rauschen
                        v += (20 * math.log10(a / 0.01 + 1)
                              * math.exp(-(d * d) / (2 * 300.0 ** 2)))
                    werte.append(v)
                self.sende_binaer(spectrum_frame(0, self.spec_fps, werte))

            # S-Meter — NUR nach ausdruecklicher Anmeldung, wie der echte
            # Server. Bis 2026-09-30 schickte die Attrappe sie ungefragt, und
            # genau daran bin ich haengengeblieben: der Signalbalken lief
            # gegen die Attrappe, blieb am echten Geraet aber leer, und ich
            # habe den Fehler eine Weile im Server gesucht statt im Client
            # (der `rx_sensors_enable:true` nie schickte).
            #
            # Eine Attrappe, die gutmuetiger ist als das Original, ist keine
            # Hilfe — sie verschiebt Fehler nach hinten, dorthin wo sie teurer
            # sind.
            # Sendezustand umschalten, wenn --sendet gesetzt ist. Der echte
            # Server meldet `trx:<rx>,<bool>` genauso, aus demselben Anlass
            # (eigenes MOX, Mikrofontaste am Geraet, CAT).
            if SENDET_ALLE > 0:
                soll = int(jetzt / SENDET_ALLE) % 2 == 1
                if soll != self.sendet:
                    self.sendet = soll
                    self.sende_text(f'trx:0,{"true" if soll else "false"};')
                    print(f'  trx:0,{"true" if soll else "false"} (--sendet)')

            if self.rx_sensors_an and jetzt - smeter_zeit > self.rx_sensors_ms / 1000.0:
                smeter_zeit = jetzt
                dbm = -83 + 9 * math.sin(jetzt / 2.2) + random.gauss(0, 1.2)
                self.sende_text(f'rx_sensors:0,{dbm:.1f};')

            time.sleep((IQ_BLOCK // 2) / IQ_RATE)

        print(f'Client {self.addr[1]} weg')
        self.sock.close()


def main():
    # Ab Werk nur auf dem eigenen Rechner. Mit `--alle` (oder HOST=0.0.0.0)
    # auch aus dem LAN erreichbar — das braucht man, sobald man die Handfunke
    # von einem echten Telefon aus prueft.
    #
    # Und zwar aus einem Grund, der nicht auf der Hand liegt: eine Seite, die
    # von einer LAN-Adresse geladen wurde, darf KEINE Verbindung nach
    # 127.0.0.1 aufbauen. Der Browser wertet das als Zugriff von einem
    # oeffentlicheren auf ein privateres Netz und blockt ihn (Private Network
    # Access). Am 2026-09-30 genau so gemessen: Seite auf
    # http://172.30.30.121:8767, WebSocket nach ws://127.0.0.1:50099 —
    # "WebSocket connection failed", ohne dass die Attrappe je etwas sah.
    # Das gilt fuer die Attrappe wie fuer Longpath selbst: beide muessen unter
    # der LAN-Adresse erreichbar sein, nicht unter localhost.
    host = os.environ.get('HOST') or (
        '0.0.0.0' if '--alle' in sys.argv else '127.0.0.1')

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((host, PORT))
    srv.listen(4)
    print(f'TCI-Attrappe lauscht auf {host}:{PORT}'
          + ('  [--still: Band ohne Stationen]' if STILL else ''))
    try:
        while True:
            sock, addr = srv.accept()
            Verbindung(sock, addr).start()
    except KeyboardInterrupt:
        pass
    finally:
        srv.close()


if __name__ == '__main__':
    main()
