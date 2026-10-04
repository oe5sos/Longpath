# Der sichere Kontext — warum das Telefon kein Mikrofon hat

**2026-10-04**

## Der Befund

Der Auftrag lautete: senden vom Telefon. Der nächste Baustein wäre die
Mikrofonaufnahme gewesen — `getUserMedia`, Abtastratenwandlung, mu-law, die
Rahmen aus `tx-ton.js` (#186).

Geschrieben wurde davon keine Zeile, denn vorher stand die Frage an, ob der
Browser am Telefon überhaupt ein Mikrofon herausgibt. Die Antwort lag seit
Wochen im Protokoll, in jeder einzelnen Meldung der Seite:

```
172.30.30.115 - - [03/Oct/2026 17:28:08] MELDUNG  anlass=knopf  …
    weg=scriptprocessor  …  sicher=false  …  ctxRate=48000
127.0.0.1     - - [03/Oct/2026 09:09:51] MELDUNG  anlass=start  …
    …                    …  sicher=true
```

`sicher` ist `window.isSecureContext`. Am Telefon: **false**.

`getUserMedia` ist `[SecureContext]`. Über `http` auf einer LAN-Adresse ist
`navigator.mediaDevices` nicht etwa leer, sondern `undefined`. Es gibt dort
kein Mikrofon — nicht „noch nicht gebaut", sondern vom Browser gesperrt.
Einen Ersatzweg wie beim ScriptProcessor gibt es nicht.

## Warum es niemandem auffiel

Am Mac ist `127.0.0.1` ohne jedes Zertifikat ein sicherer Kontext. Jede
Prüfung an der Werkbank lief also grün, und zwar aus einem Grund, der auf
dem Zielgerät nicht gilt. Die eine Stelle, an der der Unterschied zählt,
ist zugleich die einzige ohne Konsole.

Dasselbe `sicher=false` erklärt rückwirkend zwei Dinge, die bis heute als
eigenständige Eigenheiten in den Unterlagen standen:

* **`weg=scriptprocessor`.** Dass `AudioWorklet` am Telefon fehlt, war
  bekannt und als „zwei Tonwege" dokumentiert. Der Empfangston läuft dort
  seither im Hauptfaden — neben dem Wasserfall.
* **Opus fällt aus.** WebCodecs' `AudioDecoder` ist ebenfalls
  `[SecureContext]`. Im README stand das als Eigenschaft von WebCodecs, mit
  dem Schluss, eine WASM-Fremddatei lohne für 9 kB/s nicht.

Drei Posten, drei Erklärungen, ein Schalter. Dass es derselbe ist, sieht man
erst, wenn man die Fehlerliste nach der Ursache sortiert statt nach dem
Bauteil.

## Was gebaut wurde

### `tls-einrichten.sh` — eine eigene Stelle und ein Zertifikat

Zwei Stufen, nicht eine. Nur so ist am Telefon **einmal** etwas einzurichten
und nie wieder: das Serverzertifikat darf danach jederzeit erneuert werden —
neue Adresse vom Router, Ablauf nach einem Jahr — ohne dass dort noch einmal
jemand ein Profil antippt.

Im `subjectAltName` stehen der `.local`-Name (der stabile), der kurze Name,
`localhost` und die aktuellen IP-Adressen. iOS verlangt den SAN; der `CN`
allein zählt dort seit Jahren nicht mehr. Laufzeit 365 Tage, deutlich unter
Apples Deckel für Serverzertifikate.

Die Schlüssel liegen **außerhalb** des Quellbaums, unter
`~/Longpath/werkzeug/handfunke-tls`. Ein privater Schlüssel in einem
Git-Baum ist ein privater Schlüssel auf dem Weg nach GitHub.

### `handfunke-server.py` — https daneben, nicht statt

Der Seitenserver horcht jetzt zusätzlich auf Port+1 mit TLS. Fehlen die
Zertifikate, läuft alles wie bisher, und er sagt es beim Start — ein
stummer Rückfall wäre dieselbe Falle noch einmal.

`http` bleibt bestehen, und das ist kein Übergang: über `https` käme das
Telefon gar nicht erst an die Zertifizierungsstelle heran, der es vertrauen
soll. Dafür gibt es `/ca.crt`, einen einzigen fest verdrahteten Pfad
außerhalb des ausgelieferten Ordners — mit dem Typ
`application/x-x509-ca-cert`, denn mit `text/plain` zeigt Safari
Base64-Salat statt eines Installationsdialogs.

### `tci-bruecke.py` — TLS auf demselben Port

Eine `https`-Seite darf kein `ws://` mehr öffnen; der Browser sperrt das als
gemischten Inhalt. Der Fehler sieht dabei aus wie „das Funkgerät antwortet
nicht" — hätte das jemand ohne diese Untersuchung erlebt, wäre die Suche an
der falschen Stelle gewesen.

Longpaths TCI-Server spricht kein TLS und soll es nicht lernen müssen:
WSJT-X, N1MM und JTDX sprechen keines, und ein zweiter Modus im Server wäre
eine zweite Sicherungslage, die gepflegt werden will. Die Brücke ist
ohnehin ein reiner Byteweiterleiter — sie beendet das TLS und reicht
Klartext an `127.0.0.1:50001`. Der Tokenschutz des Servers gilt
unverändert; die Brücke sieht den Handschlag nie.

**Derselbe Port trägt beides.** Welches Protokoll gesprochen wird, steht im
ersten Byte: `0x16` ist der TLS-Satztyp `handshake`, ein WebSocket-Aufbau
beginnt mit dem `G` von `GET`. Abgelauscht mit `MSG_PEEK`, das Byte bleibt
liegen. Damit bleibt der alte Weg unverändert offen, und niemand muss sich
eine zweite Portnummer merken.

### `app.js` — das Schema folgt der Seite

Eine Zeile: `ws://` oder `wss://` richtet sich nach `location.protocol`.
Das ist keine Entscheidung, die der Bediener treffen soll — beide
Fehlerbilder (gemischter Inhalt, TLS gegen eine Brücke ohne Zertifikat)
sehen von außen gleich aus und wie ein Fehler des Funkgeräts.

## Geprüft

`handfunke/pruefe-bruecke-tls.py`:

```
Mit Zertifikat:
  ok    ws:// kommt unveraendert durch
  ok    wss:// kommt unveraendert durch

Gegenprobe — dieselbe Bruecke OHNE Zertifikat:
  ok    ws:// geht weiterhin
  ok    wss:// scheitert, wie es muss
```

Die Gegenprobe ist der Teil, der etwas wert ist: ohne sie belegte der grüne
Lauf nur, dass irgendetwas antwortet.

Und die ganze Kette mit einem echten WebSocket-Client,
`handfunke/pruefe-wss.mjs` — Handschlag, Aufrüstung, Textrahmen, wie die
Seite es täte:

```
  ok    wss-Handschlag steht
  ok    TCI-Begruessung kam durch (32 Zeilen)
        protocol:Longpath-Attrappe,2.0
        device:Attrappe
```

Auch hier eine Gegenprobe: derselbe Lauf **ohne** `NODE_EXTRA_CA_CERTS`
scheitert. Ein Stand, der jedes Zertifikat nimmt, prüfte nichts.

Am Seitenserver, mit `curl` gegen die eigene Stelle geprüft:
`https` liefert 200 bei `ssl_verify_result=0`, `/ca.crt` kommt
bytegleich heraus, und weder `server.key` noch `ca.key` sind über einen
Pfad erreichbar (auch nicht über `..`-Varianten).

Dabei fiel ein Fehler in der eigenen frischen Änderung auf: nach
`wrap_socket` ist der rohe Socket leer — der Deskriptor ist übernommen —
und ein `setsockopt` darauf endet mit `EBADF`, in einem Nebenfaden, also
als Zeile im Protokoll statt als Absturz. Behoben; der Grund steht als
Kommentar daneben.

## Nicht geprüft

**Das Telefon selbst.** Ob Safari das Profil annimmt, ob nach dem
Einschalten des Vertrauens `isSecureContext` auf `ja` springt, und ob dann
`AudioWorklet` und `getUserMedia` wirklich da sind — das kann nur am Gerät
entschieden werden. `kann-das-telefon.html` beantwortet alle drei Fragen auf
einen Blick und meldet das Ergebnis an den Webserver.

Bis dahin ist das hier eine begründete Erwartung, keine Messung.

## Was daraus folgt

Die Reihenfolge beim Senden ändert sich. Die Mikrofonaufnahme war als
nächster Schritt vorgesehen; sie kann am Zielgerät nicht laufen, solange
das hier nicht steht. Erst der sichere Kontext, dann das Mikrofon, dann —
nach ausdrücklicher Freigabe und an der Dummy-Last — das Tasten.

Siehe `docs/architecture/2026-10-04-handfunke-senden-und-loggen.md` für den
Gesamtentwurf.
