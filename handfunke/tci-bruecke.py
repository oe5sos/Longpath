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
ueber TCP — es muss nichts verstanden, nur durchgereicht werden.

ACHTUNG, und hier stand bis zum 2026-10-07 das Gegenteil: die Bruecke hebt
den Token-Schutz und die drei Freigaben AUS. Nicht aus Absicht, sondern aus
Bauart. Sie nimmt die Verbindung des Telefons im WLAN an und baut eine
EIGENE nach 127.0.0.1 auf -- fuer Longpath kommt damit jeder Client der App
aus Loopback. Loopback ist dort aber das Vertrauen selbst (siehe
TciClientSession::fromLoopback): kein Token, und keine der drei Freigaben
greift.

Was das praktisch heisst, solange die Bruecke laeuft:

  * `TciAllowRemoteTx=False` schuetzt nicht. An der Station haengt eine
    Antenne.
  * `TciAllowRemoteRotor=False` schuetzt nicht.
  * Ein Token wird nicht verlangt, auch wenn eines hinterlegt ist.
  * Es gilt fuer JEDES Geraet im Heimnetz, nicht nur fuer das Telefon.

Zwei Wege, das zu schliessen -- die Entscheidung gehoert dem Betreiber,
weil beide ihn etwas kosten:

  1. Den vorgesehenen Weg gehen: Setup > CAT & Network > TCI Server >
     "Bind interface" auf die Netzadresse, Token setzen, Bruecke weglassen.
     Dann greift alles wie gedacht. Kostet: das Token muss am Telefon
     eingetippt werden.
  2. Die Bruecke sagt im Handschlag, fuer wen sie kommt (ein Kopfeintrag,
     den der Server nur als EINSCHRAENKUNG liest), und Longpath behandelt
     solche Verbindungen als aus dem Netz. Kostet: eine Aenderung im
     Server, und das Telefon braucht danach ebenfalls ein Token -- oder
     eine ausdrueckliche Ausnahme dafuer.

Bis dahin gilt: die Bruecke nur im eigenen Heimnetz starten und nur,
solange sie gebraucht wird.

  python3 tci-bruecke.py                  # <Netzadresse>:50001 -> 127.0.0.1:50001
  python3 tci-bruecke.py 50010            # auf einem anderen Port horchen

Sie belegt DIESELBE Portnummer wie Longpath, nur auf der Netzadresse —
Longpath horcht auf 127.0.0.1:50001, die Bruecke auf 172.30.30.x:50001.
Das ist kein Konflikt (verschiedene Adressen) und hat einen praktischen
Grund: so lautet die Adresse fuer die Handfunke immer `<rechner>:50001`,
ob die Bruecke nun laeuft oder nicht. Eine zweite Portnummer, die der
Operator sich merken und spaeter wieder vergessen muss, entfaellt.

Seit dem 2026-10-04 beendet sie auf Wunsch auch TLS. Der Grund liegt
nicht bei der Bruecke, sondern eine Ebene hoeher: `getUserMedia` und
`AudioWorklet` brauchen einen sicheren Kontext, die Seite muss also ueber
`https` kommen -- und eine `https`-Seite darf kein `ws://` mehr oeffnen,
der Browser sperrt das als gemischten Inhalt. Longpaths TCI-Server selbst
spricht kein TLS, und er soll es auch nicht lernen muessen: WSJT-X, N1MM
und JTDX sprechen keines.

Dieselbe Portnummer traegt beides. Welches von beidem gesprochen wird,
steht im ERSTEN BYTE -- ein TLS-ClientHello beginnt mit 0x16, ein
WebSocket-Handschlag mit dem `G` von `GET`. Das wird vorsichtig abgelauscht
(MSG_PEEK, das Byte bleibt liegen) und entscheidet. So bleibt der alte Weg
unveraendert offen: wer noch ueber `http` und `ws://` kommt, merkt von der
Umstellung nichts.

Beenden mit Strg-C. Die Bruecke oeffnet das Geraet fuers lokale Netz,
solange sie laeuft, und keine Sekunde laenger.
"""

import os, socket, ssl, sys, threading, time

HORCH_PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 50001
ZIEL_PORT  = int(sys.argv[2]) if len(sys.argv) > 2 else 50001
ZIEL = ("127.0.0.1", ZIEL_PORT)

# Dieselben Dateien, die handfunke-server.py benutzt; tls-einrichten.sh legt
# sie an. Fehlen sie, laeuft die Bruecke wie bisher rein unverschluesselt.
TLS_ORDNER = os.environ.get(
    "HANDFUNKE_TLS_DIR",
    os.path.expanduser("~/Longpath/werkzeug/handfunke-tls"))


def tls_kontext():
    kette = os.path.join(TLS_ORDNER, "server-kette.crt")
    schluessel = os.path.join(TLS_ORDNER, "server.key")
    if not (os.path.exists(kette) and os.path.exists(schluessel)):
        return None
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(kette, schluessel)
    return ctx


TLS = tls_kontext()


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
        #
        # Auf einem TLS-Socket geht das nicht: dort ist "halb zu" kein
        # Zustand, den das Protokoll kennt. Ein SHUT_WR schickte das FIN
        # ohne close_notify, und der Browser meldete einen Abbruch statt
        # eines Endes. Dort bleibt der Socket offen, bis die Gegenrichtung
        # ihn ohnehin schliesst.
        if not isinstance(nach, ssl.SSLSocket):
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

    Was das Streichen selbst nicht aushebelt: den Token. Was die Bruecke als
    GANZES aushebelt, sehr wohl — siehe den Kopf dieser Datei. Hier stand
    bis zum 2026-10-07 "den Token hebelt das nicht aus", und das war richtig
    fuer diese Funktion und falsch fuer die Bruecke.
    """
    zeilen = kopf.split(b"\r\n")
    behalten = [z for z in zeilen if not z.lower().startswith(b"origin:")]
    return b"\r\n".join(behalten)


def vielleicht_tls(klient, adresse):
    """Laesst TLS zu, ohne es zu erzwingen.

    Entschieden wird am ersten Byte, und es wird nur angeschaut, nicht
    verbraucht (MSG_PEEK) -- der Handschlag, der gleich folgt, braucht es
    vollstaendig. 0x16 ist der Satztyp `handshake` aus TLS; alles andere
    ist der Klartextweg, wie bisher.
    """
    if TLS is None:
        return klient
    try:
        klient.settimeout(8)
        erst = klient.recv(1, socket.MSG_PEEK)
    except OSError:
        return klient
    finally:
        try:
            klient.settimeout(None)
        except OSError:
            pass
    if not erst or erst[0] != 0x16:
        return klient
    try:
        return TLS.wrap_socket(klient, server_side=True)
    except (ssl.SSLError, OSError) as e:
        # Haeufigster Fall: das Telefon vertraut der Stelle noch nicht und
        # bricht ab. Als Zeile im Protokoll ist das die Antwort auf "warum
        # verbindet sie nicht"; als stiller Abbruch waere es eine Stunde
        # Suche. tls-einrichten.sh sagt, was am Telefon zu tun ist.
        print(f"  {adresse[0]}: TLS-Handschlag abgebrochen ({e}) — "
              f"vertraut das Geraet der Longpath-Handfunke-CA schon?",
              flush=True)
        try:
            klient.close()
        except OSError:
            pass
        return None


def bedienen(roh, adresse):
    t0 = time.time()
    zaehler = [0, 0]
    klient = vielleicht_tls(roh, adresse)
    if klient is None:
        return
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
        # Seit Longpath die eigene Weboberflaeche selbst erkennt
        # (istEigeneHerkunft in TciServer.cpp), muss der Origin NICHT mehr
        # gestrichen werden — im Gegenteil: durchgereicht laesst er die
        # richtige Pruefung greifen, statt die Verbindung als nativen
        # Client zu tarnen. Nur gegen eine aeltere Fassung, die das noch
        # nicht kann, wird er entfernt; das zeigt sich daran, dass sie mit
        # 403 antwortet.
        ziel.sendall(erst)
    except OSError as e:
        print(f"  {adresse[0]}: Handschlag fehlgeschlagen ({e})", flush=True)
        klient.close(); ziel.close(); return
    # Auf dem UMHUELLTEN Socket, nicht auf dem rohen: `wrap_socket`
    # uebernimmt den Dateideskriptor, und das urspruengliche Objekt ist
    # danach leer. Ein setsockopt darauf endet mit EBADF, und zwar in
    # einem Nebenfaden, also als Spur im Protokoll statt als Absturz —
    # genau die Sorte Fehler, die man ohne Gegenprobe nicht sieht.
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


def horcher():
    """Oeffnet alle Sockets, auf denen die Handfunke hereinkommen kann.

    Zwei, nicht einer — und das ist der Punkt, an dem es am 2026-10-01
    noch einmal scheiterte:

      IPv4 auf der NETZADRESSE. Nicht 0.0.0.0, denn das schloesse
      127.0.0.1 ein, und das haelt Longpath selbst.

      IPv6 auf allem (`::`, aber V6ONLY). Ein `.local`-Name loest auf
      diesem Mac auf FUENF Adressen auf, und `::1` steht an erster
      Stelle. Browser probieren IPv6 zuerst; horcht dort nichts, meldet
      die Seite "keine Antwort", obwohl ueber IPv4 alles bereitstuende.
      Longpath hat gar keinen IPv6-Socket, also gibt es hier auch keinen
      Konflikt.
    """
    offen = []
    eigene = adressen()
    # Zusaetzlich der alte Port 50010. Er kostet nichts und ist der Rueckweg,
    # falls die Umstellung auf 50001 irgendwo klemmt — am 2026-10-01 lief die
    # Handfunke am iPhone bereits ueber 50010, und ein Stand, der nachts
    # umgebaut und nicht am Geraet geprueft werden konnte, darf diesen Weg
    # nicht mitnehmen.
    PORTS = [HORCH_PORT] + ([50010] if HORCH_PORT != 50010 else [])
    for port in PORTS:
        if eigene:
            v4 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            v4.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try:
                v4.bind((eigene[0], port)); v4.listen(8); offen.append((v4, f"{eigene[0]}:{port}"))
            except OSError as e:
                print(f"IPv4 {eigene[0]}:{port} nicht belegbar ({e})", flush=True)
                v4.close()
        v6 = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
        v6.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        v6.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 1)
        try:
            v6.bind(("::", port)); v6.listen(8); offen.append((v6, f"[::]:{port}"))
        except OSError as e:
            print(f"IPv6 [::]:{port} nicht belegbar ({e})", flush=True)
            v6.close()
    return offen


if __name__ == "__main__":
    import select
    offen = horcher()
    if not offen:
        print("Kein Socket zu oeffnen — horcht Longpath dort schon selbst?", flush=True)
        raise SystemExit(1)
    for _, wo in offen:
        print(f"Bruecke offen auf {wo}", flush=True)
    print(f"weitergereicht an {ZIEL[0]}:{ZIEL[1]} · Strg-C beendet sie", flush=True)
    if TLS is not None:
        print("TLS liegt bereit — derselbe Port nimmt ws:// und wss://; "
              "das erste Byte entscheidet.\n", flush=True)
    else:
        print("Ohne Zertifikat, also nur ws://. Eine https-Seite kann sich "
              "damit nicht verbinden (gemischter Inhalt); "
              "handfunke/tls-einrichten.sh legt eines an.\n", flush=True)
    try:
        while True:
            bereit, _, _ = select.select([s for s, _ in offen], [], [], 1.0)
            for srv in bereit:
                k, a = srv.accept()
                threading.Thread(target=bedienen, args=(k, a), daemon=True).start()
    except KeyboardInterrupt:
        print("\nBruecke zu.", flush=True)
    finally:
        for srv, _ in offen:
            srv.close()
