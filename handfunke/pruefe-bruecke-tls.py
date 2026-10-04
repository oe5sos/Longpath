#!/usr/bin/env python3
"""Prueft die TLS-Erkennung der Bruecke — ohne Funkgeraet, ohne Telefon.

Die Frage ist nicht "geht TLS", sondern "geht TLS, OHNE den alten Weg zu
brechen". Ein Port, zwei Protokolle, entschieden am ersten Byte: wenn das
schiefgeht, verliert entweder das Telefon seine Verbindung oder jeder
bisherige Client seine.

Die Gegenprobe ist der Teil, der etwas wert ist. Dieselben zwei Laeufe
noch einmal gegen eine Bruecke OHNE Zertifikat; dort MUSS der TLS-Lauf
scheitern. Waere auch der gruen, pruefte dieser Stand gar nichts — man
saehe nur, dass irgendetwas antwortet.

    python3 pruefe-bruecke-tls.py

Vorher muss tls-einrichten.sh gelaufen sein.
"""

import os, socket, ssl, subprocess, sys, threading, time

HIER = os.path.dirname(os.path.abspath(__file__))
TLS_ECHT = os.environ.get(
    "HANDFUNKE_TLS_DIR", os.path.expanduser("~/Longpath/werkzeug/handfunke-tls"))
CA = os.path.join(TLS_ECHT, "ca.crt")

# Hoch genug, um nicht mit Longpath (50001) oder dem Rueckweg (50010) zu
# kollidieren, und ausserhalb dessen, was die Bruecke selbst zusaetzlich
# oeffnet.
ZIEL_PORT = 50093
BRUECKE_PORT = 50094


def echo_ziel(bereit):
    """Spielt Longpath: nimmt an und schickt zurueck, was ankommt."""
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", ZIEL_PORT))
    s.listen(8)
    bereit.set()
    while True:
        k, _ = s.accept()

        def bedien(k=k):
            try:
                while True:
                    d = k.recv(4096)
                    if not d:
                        break
                    k.sendall(b"ECHO:" + d)
            except OSError:
                pass
            finally:
                k.close()

        threading.Thread(target=bedien, daemon=True).start()


def eigene_adresse():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.connect(("10.255.255.255", 1))
    a = s.getsockname()[0]
    s.close()
    return a


def lauf(mit_tls, tls_dir):
    """Startet eine Bruecke, schickt einen Handschlag durch, raeumt auf."""
    umg = dict(os.environ, HANDFUNKE_TLS_DIR=tls_dir)
    p = subprocess.Popen(
        [sys.executable, os.path.join(HIER, "tci-bruecke.py"),
         str(BRUECKE_PORT), str(ZIEL_PORT)],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=umg, text=True)
    time.sleep(1.5)
    adresse = eigene_adresse()
    try:
        roh = socket.create_connection((adresse, BRUECKE_PORT), timeout=5)
        if mit_tls:
            ctx = ssl.create_default_context(cafile=CA)
            k = ctx.wrap_socket(roh, server_hostname=adresse)
        else:
            k = roh
        gruss = b"GET /tci HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\n\r\n"
        k.sendall(gruss)
        k.settimeout(5)
        antwort = k.recv(4096)
        k.close()
        return (antwort == b"ECHO:" + gruss), antwort[:40]
    except Exception as e:
        return False, repr(e)[:90]
    finally:
        p.terminate()
        try:
            p.wait(timeout=5)
        except subprocess.TimeoutExpired:
            p.kill()


fehler = 0


def pruefe(name, ok, text=""):
    global fehler
    print(f"  {'ok   ' if ok else 'FEHLT'} {name}" + (f": {text}" if not ok and text else ""))
    if not ok:
        fehler += 1


if __name__ == "__main__":
    if not os.path.exists(CA):
        print(f"Kein ca.crt in {TLS_ECHT} — zuerst tls-einrichten.sh laufen lassen.")
        sys.exit(2)

    bereit = threading.Event()
    threading.Thread(target=echo_ziel, args=(bereit,), daemon=True).start()
    if not bereit.wait(5):
        print("Das Echo-Ziel kam nicht hoch.")
        sys.exit(2)

    leer = os.path.join(HIER, ".kein-tls")   # existiert, enthaelt nichts
    os.makedirs(leer, exist_ok=True)

    print("Bruecke MIT Zertifikat:")
    ok, t = lauf(False, TLS_ECHT); pruefe("ws://  kommt unveraendert durch", ok, str(t))
    ok, t = lauf(True,  TLS_ECHT); pruefe("wss:// kommt unveraendert durch", ok, str(t))

    print("\nGegenprobe — dieselbe Bruecke OHNE Zertifikat:")
    ok, t = lauf(False, leer); pruefe("ws://  geht weiterhin", ok, str(t))
    ok, t = lauf(True,  leer); pruefe("wss:// scheitert, wie es muss", not ok,
                                      "es kam durch — dann prueft dieser Stand nichts")

    os.rmdir(leer)
    print()
    print("alles gruen" if fehler == 0 else f"{fehler} Punkt(e) offen")
    sys.exit(1 if fehler else 0)
