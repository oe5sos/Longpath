#!/usr/bin/env python3
"""handfunke-server.py — liefert die Handfunke im Heimnetz aus.

Laeuft als LaunchAgent (at.longpath.handfunke), damit die Seite da ist,
sobald der Mac angemeldet ist — ohne dass jemand ein Fenster offen haelt.

Zwei Dinge, die ein nackter `python3 -m http.server` nicht tut:

  * `Cache-Control: no-store`. Safari auf dem iPhone haelt einmal geladene
    Dateien sonst zaeh fest; am 2026-09-30 lief darum eine alte app.js
    weiter, waehrend am Mac laengst die neue lag, und der Fehler war nicht
    zu finden, weil er gar nicht mehr im Code stand.
  * Saubere Typen fuer .webmanifest und .js, sonst nimmt iOS das
    Startbildschirm-Symbol nicht an.
  * Eine Annahmestelle fuer `/melde`. Ein Telefon hat keine Konsole, die
    jemand lesen koennte; die Seite meldet darum ihren Zustand hierher.

Und seit dem 2026-10-04 liefert er die Seite zusaetzlich ueber `https`.
Das ist keine Zierde: `getUserMedia` und `AudioWorklet` sind
**[SecureContext]** und fallen ueber `http` auf einer LAN-Adresse komplett
aus. Das Telefon meldet das selbst, seit es die Annahmestelle gibt --
`sicher=false` in jeder Zeile von 172.30.30.x, `sicher=true` nur von
127.0.0.1. Deshalb laeuft der Empfangston dort bis heute ueber den
ScriptProcessor, und deshalb gibt es dort kein Mikrofon. Zertifikate legt
`tls-einrichten.sh` an; fehlen sie, bleibt es beim reinen `http`, und der
Server sagt das beim Start.

`http` bleibt daneben bestehen, und das ist Absicht: ueber `https` kaeme das
Telefon gar nicht erst an die Zertifizierungsstelle heran, der es vertrauen
soll. Genau dafuer gibt es `/ca.crt`.

Zur Annahmestelle: bis zum 2026-10-02 gab es sie nicht, und die Meldung
landete als 404 im Protokoll — sichtbar nur, WEIL sie scheiterte (unten
schreibt log_message ausschliesslich 4xx und 5xx). Das hat zweimal an einem
Tag den Fehler gefunden: einmal einen schlafenden AudioContext, einmal einen
Tonstrom, der mit vollen 12 kB/s Stille trug. Ein naiver "Fix", der nur 204
zurueckgibt, haette sie stumm gemacht — darum wird hier ausdruecklich
protokolliert, und zwar lesbar statt als Fragezeichenkette.
"""

import http.server, os, socket, socketserver, ssl, sys, threading, urllib.parse

ORDNER = os.path.dirname(os.path.abspath(__file__))
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8771
PORT_TLS = int(sys.argv[2]) if len(sys.argv) > 2 else PORT + 1

# Wo tls-einrichten.sh seine Dateien ablegt. Ausserhalb des Quellbaums --
# ein privater Schluessel in einem Git-Baum ist ein privater Schluessel auf
# dem Weg nach GitHub.
TLS_ORDNER = os.environ.get(
    "HANDFUNKE_TLS_DIR",
    os.path.expanduser("~/Longpath/werkzeug/handfunke-tls"))
TLS_KETTE = os.path.join(TLS_ORDNER, "server-kette.crt")
TLS_SCHLUESSEL = os.path.join(TLS_ORDNER, "server.key")
TLS_CA = os.path.join(TLS_ORDNER, "ca.crt")


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        ".js": "text/javascript",
        ".mjs": "text/javascript",
        ".webmanifest": "application/manifest+json",
        ".json": "application/json",
    }

    def __init__(self, *a, **kw):
        super().__init__(*a, directory=ORDNER, **kw)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store, must-revalidate")
        super().end_headers()

    def do_GET(self):
        weg = self.path.split("?", 1)[0]
        if weg == "/melde":
            self.melde_annehmen()
            return
        if weg == "/ca.crt":
            self.ca_ausliefern()
            return
        super().do_GET()

    # Die Zertifizierungsstelle zum Abholen. Sie liegt ausserhalb des
    # Ordners, den dieser Server sonst ausliefert, und muss darum von Hand
    # herausgereicht werden -- nur sie, nichts daneben, und niemals der
    # private Schluessel: ausgeliefert wird ausschliesslich dieser eine
    # fest verdrahtete Pfad.
    #
    # Der Typ ist Absicht: mit application/x-x509-ca-cert bietet iOS das
    # Profil zur Installation an, mit text/plain zeigt Safari Base64-Salat.
    def ca_ausliefern(self):
        try:
            with open(TLS_CA, "rb") as f:
                daten = f.read()
        except OSError:
            self.send_error(404, "noch keine Zertifizierungsstelle -- "
                                 "tls-einrichten.sh ausfuehren")
            return
        self.send_response(200)
        self.send_header("Content-Type", "application/x-x509-ca-cert")
        self.send_header("Content-Length", str(len(daten)))
        self.send_header("Content-Disposition",
                         'attachment; filename="longpath-handfunke-ca.crt"')
        self.end_headers()
        self.wfile.write(daten)

    # Die Zustandsmeldung der Seite: eine Zeile, die man lesen kann.
    #
    # Reihenfolge ist Absicht — was zuerst kommt, beantwortet die Frage
    # "kommt ueberhaupt etwas an, und laeuft die Tonkette?" am schnellsten:
    #   ready   Verbindung steht und der Init-Burst ist durch
    #   ctx     Zustand des AudioContext (suspended = schlaeft, kein Ton)
    #   rahmen  empfangene Tonrahmen
    #   vorrat  Fuellstand des Rings; steht er am Anschlag, wird nicht geleert
    #   takte   wie oft die Tonausgabe gerufen wurde
    FELDER = ("anlass", "ready", "ctx", "sitzung", "rahmen", "vorrat", "ziel",
              "takte", "leer", "rate", "tonTyp", "weg", "af", "vorDeckel",
              "nachDeckel", "hfdb", "sicher", "fehler")

    def melde_annehmen(self):
        roh = urllib.parse.urlparse(self.path).query
        felder = urllib.parse.parse_qs(roh, keep_blank_values=True)
        teile = []
        for name in self.FELDER:
            if name in felder:
                teile.append(f"{name}={felder[name][0]}")
        # Was die Seite sonst noch mitschickt, hinten anhaengen statt
        # verschlucken: eine neue Kennzahl soll nicht erst hier eingetragen
        # werden muessen, um sichtbar zu sein.
        for name in sorted(felder):
            if name not in self.FELDER:
                teile.append(f"{name}={felder[name][0]}")
        super().log_message("MELDUNG  %s", "  ".join(teile))
        self.send_response(204)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def log_message(self, fmt, *args):
        # Nur Fehler, sonst laeuft das Protokoll mit jedem Spektrumbild voll.
        if args and str(args[1]).startswith(("4", "5")):
            super().log_message(fmt, *args)


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def tls_kontext():
    """Der TLS-Kontext, oder None, solange es keine Zertifikate gibt.

    Fehlende Zertifikate sind kein Fehler -- sie sind der Zustand vor dem
    ersten Lauf von tls-einrichten.sh. Der Server laeuft dann wie bisher
    ueber http weiter; nur Mikrofon und AudioWorklet bleiben am Telefon
    aus.
    """
    if not (os.path.exists(TLS_KETTE) and os.path.exists(TLS_SCHLUESSEL)):
        return None
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(TLS_KETTE, TLS_SCHLUESSEL)
    return ctx


if __name__ == "__main__":
    name = socket.gethostname().split(".")[0].lower()
    ctx = tls_kontext()

    # Der https-Horcher laeuft im Nebenfaden, der http-Horcher im
    # Hauptfaden -- so beendet Strg-C weiterhin beides, und der Dienst
    # haengt nicht an einem Faden, den niemand abraeumt.
    if ctx is not None:
        tls_srv = Server(("0.0.0.0", PORT_TLS), Handler)
        tls_srv.socket = ctx.wrap_socket(tls_srv.socket, server_side=True)
        threading.Thread(target=tls_srv.serve_forever, daemon=True).start()
        print(f"Handfunke auf https://{name}.local:{PORT_TLS}/index.html "
              f"(sicherer Kontext -- Mikrofon und AudioWorklet moeglich)",
              flush=True)
    else:
        print("Kein Zertifikat gefunden -- nur http. Am Telefon bleiben "
              "Mikrofon und AudioWorklet damit aus; "
              "handfunke/tls-einrichten.sh legt eines an.", flush=True)

    print(f"Handfunke auf http://{name}.local:{PORT}/index.html", flush=True)
    with Server(("0.0.0.0", PORT), Handler) as srv:
        srv.serve_forever()
