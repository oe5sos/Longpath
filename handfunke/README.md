# Longpath Handfunke

Die Fernbedienung fürs Telefon. Eine Seite, die Safari über
*Teilen → Zum Home-Bildschirm* zu einer App mit eigenem Symbol macht — ohne
App Store, ohne Xcode, ohne Apple-Entwicklerprogramm.

Sie spricht Longpaths eingebauten TCI-Server. Kein Bauwerkzeug, keine
Abhängigkeiten, nichts zu übersetzen.

## Warum kein natives Programm

Weil ein Browser eine Sendetaste nicht verantworten kann: iOS friert eine
Safari-Seite mitten im Senden ein, ohne `close` zu feuern — die Seite kann
dann nicht mehr zusagen, MOX zu lösen. Empfang, Bedienung und Ton laufen hier;
**Senden gehört in ein natives Programm**, das das Betriebssystem am Leben
hält. Deshalb steht die Sendetaste hier gesperrt.

Serverseitig ist der Fall trotzdem abgesichert: der Sendezeit-Deckel
(`TciMaxTransmitSeconds`, Vorgabe 180 s) und der Wachhund des tastenden
Clients werfen einen hängenden Sender ab, ganz gleich welcher Client ihn
getastet hat.

## Aufbau

| Datei           | Aufgabe |
|-----------------|---------|
| `index.html`    | Die Seite. Enthält nur Gerüst, keine Logik. |
| `stil.css`      | Der Hausstil, 1:1 aus `StyleConstants.h` übernommen. |
| `tci.js`        | Der Draht: WebSocket, Textkanal, Binärrahmen, FFT. |
| `app.js`        | Anzeige und Bedienung. |
| `rx-worklet.js` | Empfangston im Audio-Faden, mit Ringpuffer. |
| `attrappe.py`   | Prüfstand: ein TCI-Server aus Pappe, für Läufe ohne Funkgerät. |

### Die eine Regel

**Die Seite hält keinen eigenen Zustand.** Alles, was angezeigt wird, kommt
aus einer Zeile des Servers. Ein Knopfdruck schickt einen Befehl und wartet
auf die Gegenmeldung, statt die Anzeige vorher umzustellen. Das kostet einen
Wimpernschlag und erspart die Klasse von Fehlern, bei der das Telefon etwas
anderes behauptet als das Gerät tut.

Einzige Ausnahme, benannt im Code: die Schalter unter *Aufbereitung*, die der
Server nicht zurückmeldet.

## Benutzen

1. In Longpath: **Setup → CAT & Network → TCI Server** einschalten und
   *Bind interface* auf die **konkrete WLAN-Adresse** stellen — nicht auf
   `0.0.0.0`.
2. Die Herkunft der Seite in `TciAllowedOrigins` eintragen. Ohne das weist der
   Server sie ab: WebSocket kennt keine Gleiche-Herkunft-Regel, deshalb prüft
   Longpath sie selbst (siehe `TciServer::start`).
3. Am Telefon die Seite aufrufen, Adresse eingeben, *Zum Home-Bildschirm*.

## Ohne Funkgerät prüfen

```
python3 handfunke/attrappe.py         # lauscht auf 127.0.0.1:50099
python3 -m http.server 8766 -d handfunke
```

Die Attrappe liefert Init-Burst, ein Spektrum mit sechs Trägern (einer setzt
aus, damit der Wasserfall etwas zu erzählen hat), Empfangston und ein
wanderndes S-Meter. Sie ersetzt Longpath nicht — sie prüft die Naht.

## Was noch fehlt

- **Der schmale Spektrumrahmen.** Heute rechnet das Telefon die FFT selbst aus
  rohem I/Q. Gemessen: 404 kB/s bei 48 kHz, hochgerechnet rund 1,5 MB/s bei
  192 kHz. Das trägt im WLAN und nicht unterwegs. Ein eigener Rahmen mit
  fertigen Bins (8 bit je Bin, Punktzahl aus der Pixelbreite) drückt das auf
  rund 27 kB/s.
- **Ton-Kompression.** Opus mono 48 kHz in 20-ms-Rahmen statt float32-Stereo.
- **Senden**, siehe oben.
- **Antenne, Mikrofonpegel, Bandwechsel** als TCI-Befehle — die gibt es
  serverseitig noch nicht.
