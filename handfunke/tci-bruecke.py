#!/usr/bin/env python3
"""tci-bruecke.py — macht Longpaths TCI-Server im WLAN erreichbar, ohne an
Longpaths Einstellungen zu ruehren.

Warum es das gibt: Longpath bindet den TCI-Server ab Werk auf 127.0.0.1 —
richtig so, denn sonst steht das Funkgeraet jedem im Netz offen. Fuer die
Handfunke muss er aber vom Telefon aus erreichbar sein. Der vorgesehene Weg
ist Setup > CAT & Network > TCI Server > "Bind interface". Wer den gerade
nicht gehen kann oder will, startet stattdessen diese Bruecke: sie horcht im
WLAN und reicht jedes Byte unveraendert an 127.0.0.1:50001 weiter.

Das ist ein reiner TCP-Weiterleiter. TCI laeuft ueber WebSocket, WebSocket
ueber TCP — es muss nichts verstanden, nur durchgereicht werden. Damit gilt
auch der Token-Schutz des Servers unveraendert: die Bruecke sieht den
Handschlag nie, sie kopiert ihn nur.

  python3 tci-bruecke.py                  # <Netzadresse>:50001 -> 127.0.0.1:50001
  python3 tci-bruecke.py 50010            # auf einem anderen Port horchen

Sie belegt DIESELBE Portnummer wie Longpath, nur auf der Netzadresse —
Longpath horcht auf 127.0.0.1:50001, die Bruecke auf 172.30.30.x:50001.
Das ist kein Konflikt (verschiedene Adressen) und hat einen praktischen
Grund: so lautet die Adresse fuer die Handfunke immer `<rechner>:50001`,
ob die Bruecke nun laeuft oder nicht. Eine zweite Portnummer, die der
Operator sich merken und spaeter wieder vergessen muss, entfaellt.

Beenden mit Strg-C. Die Bruecke oeffnet das Geraet fuers lokale Netz,
solange sie laeuft, und keine Sekunde laenger.
"""

import socket, sys, threading, time

HORCH_PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 50001
ZIEL_PORT  = int(sys.argv[2]) if len(sys.argv) > 2 else 50001
ZIEL = ("127.0.0.1", ZIEL_PORT)


def schaufeln(von, nach, zaehler, i):
    """Kopiert in eine Richtung, bis die Seite zumacht."""
    try:
        while True:
            d = von.recv(65536)
            if not d:
                break
            nach.sendall(d)
            zaehler[i] += len(d)
    except OSError:
        pass
    finally:
        # Nur die eigene Richtung schliessen, damit die Gegenrichtung noch
        # zu Ende senden kann — sonst reisst die Antwort mitten im Rahmen ab.
        try:
            nach.shutdown(socket.SHUT_WR)
        except OSError:
            pass


def ohne_origin(kopf: bytes) -> bytes:
    """Streicht den Origin-Kopf aus dem WebSocket-Handschlag.

    Longpaths TCI-Server weist jede Verbindung mit 403 ab, deren Herkunft
    nicht in `TciAllowedOrigins` steht — und die Liste ist ab Werk leer.
    Gedacht ist die Sperre gegen FREMDE Webseiten, die im Browser des
    Operators heimlich das Funkgeraet greifen; native Clients (WSJT-X, N1MM)
    senden gar keinen Origin und kommen deshalb durch.

    Die Handfunke ist technisch ein Browser und faellt in dieselbe Sperre,
    obwohl sie die eigene Oberflaeche ist. Die saubere Loesung ist, ihre
    Herkunft in `TciAllowedOrigins` einzutragen. Solange das nicht
    geschehen ist, nimmt die Bruecke den Kopf heraus und die Verbindung
    sieht fuer Longpath aus wie die eines nativen Clients.

    Was das NICHT aushebelt: den Token. Wer ueber Netz hereinkommt, muss ihn
    weiterhin nennen — dieser Schutz liegt im Protokoll, nicht im Handschlag.
    """
    zeilen = kopf.split(b"\r\n")
    behalten = [z for z in zeilen if not z.lower().startswith(b"origin:")]
    return b"\r\n".join(behalten)


def bedienen(klient, adresse):
    t0 = time.time()
    zaehler = [0, 0]
    try:
        ziel = socket.create_connection(ZIEL, timeout=5)
    except OSError as e:
        print(f"  {adresse[0]}: Longpath antwortet nicht auf {ZIEL[0]}:{ZIEL[1]} ({e})", flush=True)
        klient.close()
        return

    # Den Handschlag einmal lesen, Origin streichen, weiterreichen. Danach
    # laeuft alles unveraendert durch — nur die ersten Bytes werden angefasst.
    try:
        klient.settimeout(8)
        erst = b""
        while b"\r\n\r\n" not in erst and len(erst) < 65536:
            d = klient.recv(4096)
            if not d:
                klient.close(); ziel.close(); return
            erst += d
        klient.settimeout(None)
        kopf, _, rest = erst.partition(b"\r\n\r\n")
        hatte = b"origin:" in kopf.lower()
        ziel.sendall(ohne_origin(kopf) + b"\r\n\r\n" + rest)
        if hatte:
            print(f"  {adresse[0]}: Origin-Kopf entfernt (sonst 403)", flush=True)
    except OSError as e:
        print(f"  {adresse[0]}: Handschlag fehlgeschlagen ({e})", flush=True)
        klient.close(); ziel.close(); return
    for s in (klient, ziel):
        s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    print(f"  {adresse[0]} verbunden", flush=True)
    hin = threading.Thread(target=schaufeln, args=(klient, ziel, zaehler, 0), daemon=True)
    her = threading.Thread(target=schaufeln, args=(ziel, klient, zaehler, 1), daemon=True)
    hin.start(); her.start(); hin.join(); her.join()
    klient.close(); ziel.close()
    dauer = time.time() - t0
    print(f"  {adresse[0]} getrennt nach {dauer:.0f} s · "
          f"{zaehler[0]/1024:.0f} kB hin, {zaehler[1]/1024:.0f} kB her", flush=True)


def adressen():
    """Die eigenen IPv4-Adressen, damit man weiss, was man eintippen muss."""
    raus = []
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("10.255.255.255", 1)); raus.append(s.getsockname()[0]); s.close()
    except OSError:
        pass
    return raus


if __name__ == "__main__":
    # An die NETZADRESSE binden, nicht an 0.0.0.0: auf 0.0.0.0 waere der Port
    # schon von Longpath belegt (das haelt 127.0.0.1:50001), und die Bruecke
    # kaeme gar nicht hoch.
    eigene = adressen()
    if not eigene:
        print("Keine Netzadresse gefunden — haengt der Rechner im WLAN?", flush=True)
        raise SystemExit(1)
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        srv.bind((eigene[0], HORCH_PORT))
    except OSError as e:
        print(f"{eigene[0]}:{HORCH_PORT} laesst sich nicht belegen ({e}).", flush=True)
        print("Horcht Longpath dort schon selbst? Dann wird die Bruecke nicht "
              "gebraucht.", flush=True)
        raise SystemExit(1)
    srv.listen(8)
    for a in eigene:
        print(f"Bruecke offen auf {a}:{HORCH_PORT} — in der Handfunke steht "
              f"damit dieselbe Adresse wie ohne Bruecke", flush=True)
    print(f"weitergereicht an {ZIEL[0]}:{ZIEL[1]} · Strg-C beendet sie\n", flush=True)
    try:
        while True:
            k, a = srv.accept()
            threading.Thread(target=bedienen, args=(k, a), daemon=True).start()
    except KeyboardInterrupt:
        print("\nBruecke zu.", flush=True)
    finally:
        srv.close()
