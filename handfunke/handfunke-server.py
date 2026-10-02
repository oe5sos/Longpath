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
"""

import http.server, os, socket, socketserver, sys

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
