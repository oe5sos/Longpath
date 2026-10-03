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

Zur Annahmestelle: bis zum 2026-10-02 gab es sie nicht, und die Meldung
landete als 404 im Protokoll — sichtbar nur, WEIL sie scheiterte (unten
schreibt log_message ausschliesslich 4xx und 5xx). Das hat zweimal an einem
Tag den Fehler gefunden: einmal einen schlafenden AudioContext, einmal einen
Tonstrom, der mit vollen 12 kB/s Stille trug. Ein naiver "Fix", der nur 204
zurueckgibt, haette sie stumm gemacht — darum wird hier ausdruecklich
protokolliert, und zwar lesbar statt als Fragezeichenkette.
"""

import http.server, os, socket, socketserver, sys, urllib.parse

ORDNER = os.path.dirname(os.path.abspath(__file__))
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8771


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
        if self.path.split("?", 1)[0] == "/melde":
            self.melde_annehmen()
            return
        super().do_GET()

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


if __name__ == "__main__":
    name = socket.gethostname().split(".")[0].lower()
    print(f"Handfunke auf http://{name}.local:{PORT}/index.html", flush=True)
    with Server(("0.0.0.0", PORT), Handler) as srv:
        srv.serve_forever()
