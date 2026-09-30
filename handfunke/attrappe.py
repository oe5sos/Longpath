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

import base64, hashlib, math, random, socket, struct, threading, time, sys

PORT = 50099
IQ_RATE = 48000          # bewusst klein: die Attrappe soll die Naht pruefen,
AUDIO_RATE = 48000       # nicht die Bandbreite
IQ_BLOCK = 4096          # Werte je Rahmen (I und Q zusammen) -> 2048 Paare
AUDIO_BLOCK = 2048       # Werte je Rahmen (L und R zusammen) -> 1024 Paare

MAGIC = b'258EAFA5-E914-47DA-95CA-C5AB0DC85B11'

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


def stream_frame(receiver, sample_rate, stream_type, values):
    """64-Byte-Kopf + float32-Nutzlast, genau wie TciBinaryFrame."""
    hdr = [0] * 16
    hdr[0] = receiver
    hdr[1] = sample_rate
    hdr[2] = 3                 # Float32
    hdr[5] = len(values)       # ALLE Werte, nicht je Kanal
    hdr[6] = stream_type
    hdr[7] = 2                 # Kanaele
    return b''.join(struct.pack('<I', v) for v in hdr) + \
           struct.pack(f'<{len(values)}f', *values)


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
        self.spec_zeit = 0.0
        self.phase = 0.0
        self.tonphase = 0.0

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
        if name == 'iq_start':
            self.iq_an = True
            print(f'  {self.addr[1]}: IQ an')
        elif name == 'iq_stop':
            self.iq_an = False
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
            self.sende_text(
                f'spectrum_start:{args[0]},{self.spec_punkte},{self.spec_fps};')
            print(f'  {self.addr[1]}: Spektrum an, {self.spec_punkte} Punkte,'
                  f' {self.spec_fps} B/s')
        elif name == 'spectrum_stop':
            self.spec_an = False
            self.sende_text(f'spectrum_stop:{args[0]};')
        elif name in ('drive', 'tune_drive', 'vfo', 'modulation',
                      'rx_filter_band', 'trx', 'tune'):
            # Wie der echte Server: setzen und die Gegenmeldung schicken,
            # damit der Client seinen Zustand NUR vom Server bekommt.
            if len(args) >= 2:
                self.sende_text(f'{name}:{",".join(args)};')
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
        traeger = [(-14000, 0.22, 0.0), (-6200, 0.09, 0.0), (-1500, 0.45, 0.0),
                   (3100, 0.13, 0.7), (8800, 0.30, 0.0), (15500, 0.06, 1.3)]
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
                werte = []
                for i in range(AUDIO_BLOCK // 2):
                    t = self.tonphase + i / AUDIO_RATE
                    # Ein ruhiger Zweiklang, damit man hoert, dass es laeuft,
                    # ohne dass es nach Alarm klingt.
                    v = 0.09 * math.sin(2 * math.pi * 620 * t) \
                      + 0.05 * math.sin(2 * math.pi * 930 * t) \
                      + random.gauss(0, 0.004)
                    werte.append(v)
                    werte.append(v)
                self.tonphase += (AUDIO_BLOCK // 2) / AUDIO_RATE
                self.sende_binaer(stream_frame(0, AUDIO_RATE, 1, werte))

            if self.spec_an and (jetzt - self.spec_zeit) >= 1.0 / self.spec_fps:
                self.spec_zeit = jetzt
                p = self.spec_punkte
                spanne = IQ_RATE                      # volle Breite
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

            # S-Meter, wie der echte Server alle 200 ms
            if jetzt - smeter_zeit > 0.2:
                smeter_zeit = jetzt
                dbm = -83 + 9 * math.sin(jetzt / 2.2) + random.gauss(0, 1.2)
                self.sende_text(f'rx_sensors:0,{dbm:.1f};')

            time.sleep((IQ_BLOCK // 2) / IQ_RATE)

        print(f'Client {self.addr[1]} weg')
        self.sock.close()


def main():
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(('127.0.0.1', PORT))
    srv.listen(4)
    print(f'TCI-Attrappe lauscht auf 127.0.0.1:{PORT}')
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
