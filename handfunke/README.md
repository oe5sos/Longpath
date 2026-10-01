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
hält. Deshalb sind **beide** Sendetasten gesperrt — auch der Abstimmträger.
Der heisst nicht „senden", legt aber einen Dauerträger mit voller Leistung
auf die Antenne. Bis zum 2026-09-30 war er hier bedienbar, während die
SENDEN-Taste daneben ausdrücklich tot war; das war ein Fehler und ist behoben.

Serverseitig ist der Fall abgesichert: der Sendezeit-Deckel
(`TciMaxTransmitSeconds`, Vorgabe 180 s) und der Wachhund des tastenden
Clients werfen einen hängenden Sender ab, ganz gleich welcher Client ihn
getastet hat — seit dem 2026-09-30 auch beim Abstimmträger. Vorher hing
beides allein am `trx:`-Weg, und ein vom Netz getasteter Träger blieb
stehen, wenn der Client verschwand. Diese Zusage stand hier also schon,
bevor sie stimmte; jetzt hält sie.

## Aufbau

| Datei           | Aufgabe |
|-----------------|---------|
| `index.html`    | Die Seite. Enthält nur Gerüst, keine Logik. |
| `stil.css`      | Der Hausstil, 1:1 aus `StyleConstants.h` übernommen. |
| `tci.js`        | Der Draht: WebSocket, Textkanal, Binärrahmen, FFT. |
| `app.js`        | Anzeige und Bedienung. |
| `rx-worklet.js` | Empfangston im Audio-Faden. Hülle um `ton-kern.js`. |
| `ton-kern.js`   | Ringpuffer und Hochtastung. Wird von BEIDEN Tonwegen benutzt — dem Worklet und dem Ersatzweg (siehe unten), damit die Rechnung nicht zweimal dasteht und auseinanderläuft. |
| `attrappe.py`   | Prüfstand: ein TCI-Server aus Pappe, für Läufe ohne Funkgerät. |
| `kann-das-telefon.html` | Selbstauskunft eines Geräts: sicherer Kontext, AudioWorklet, WebCodecs, Bildschirmmaße. Meldet das Ergebnis an den Webserver, statt es abtippen zu lassen. |

### Zwei Tonwege

`AudioWorklet` ist **[SecureContext]** und fehlt auf einer LAN-Adresse über
`http` — also genau dort, wo das Telefon die Seite holt. Dann greift der
Ersatzweg über `ScriptProcessorNode`. Der läuft im Hauptfaden statt im
Tonfaden und kann unter Last rauh werden; stumm wäre erheblich schlimmer.

Zwei Fallen dabei, beide am echten Gerät gefunden:

* Ein ScriptProcessor **ohne Eingangskanal** wird von Safari nicht getaktet.
  Er bekommt deshalb einen Eingang und eine stille Quelle davor.
* iOS stuft Web-Audio als *Ambient* ein, und diese Kategorie gehorcht dem
  Stummschalter am Gerät — Kopfhörer ausgenommen. Ein stilles `<audio>`-
  Element bringt die Seite in die Kategorie *Playback*, die ihn ignoriert.

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
   Loopback, sonst kommt das Telefon nicht hin.
2. Dort unter *Remote access*: **Token** → `New` erzeugt einen
   Kopplungscode aus acht Zeichen (`ABCD-EFGH`). Beim Eintippen sind
   Bindestrich und Groß-/Kleinschreibung egal.
3. Ebendort **Allowed pages**: die Adresse eintragen, von der die Seite
   kommt, etwa `http://192.168.1.10:8767`. Ohne das weist der Server sie ab —
   WebSocket kennt keine Gleiche-Herkunft-Regel, deshalb prüft Longpath sie
   selbst (`TciServer::start`). Native Clients wie WSJT-X schicken keine
   Herkunft und sind davon nicht betroffen.
4. Am Telefon die Seite aufrufen, Adresse und Code eingeben,
   *Zum Home-Bildschirm*.
5. **Einmal auf `TON EIN` tippen.** iOS gibt Ton erst nach einer Berührung
   frei; der Knopf verschwindet, sobald er läuft.

### Bedienung

* **Tippen** im Spektrum oder Wasserfall springt auf die Stelle.
* **Wischen** stimmt ab. Dabei friert das Bild ein und der Zeiger wandert —
  so sieht man, wohin man fährt. Longpath führt die Bildmitte sonst der
  Abstimmung nach, und dann ändert sich sichtbar nur die Zahl.
* **Kneifen** vergrößert, von 192 bis 6 kHz. Der Ausschnitt wird am Server
  genommen, bevor verdichtet wird — nur so steigt die Auflösung wirklich
  (bei 6 kHz sind es 16 Hz je Bildpunkt statt 515).
* **Lange auf die Datenrate tippen** spielt einen Testton direkt an den
  Ausgang, am Lautstärkeregler vorbei. Trennt „das Gerät gibt nichts aus"
  von „unsere Daten taugen nicht".

### Was es kostet

Gemessen am echten Gerät (ANVELINA Pro 3, 12 kHz mono mu-law):

| | kB/s |
|---|---|
| Ton | 12,9 |
| Bild (373 Punkte) | 4,2 |
| Messwerte | 0,5 |
| **gesamt** | **17,6** — rund 63 MB je Stunde |

Roher I/Q wäre an derselben Stelle über 400 kB/s.

## Ohne Funkgerät prüfen

```
python3 handfunke/attrappe.py         # lauscht auf 127.0.0.1:50099
python3 -m http.server 8766 -d handfunke
```

Die Attrappe liefert Init-Burst, ein Spektrum mit sechs Trägern (einer setzt
aus, damit der Wasserfall etwas zu erzählen hat), Empfangston und ein
wanderndes S-Meter. Sie ersetzt Longpath nicht — sie prüft die Naht.

## Was noch fehlt

* **Senden.** Beide Sendetasten sind tot, auch `TUNE` — der Abstimmträger
  heißt nicht so, legt aber einen Dauerträger auf die Antenne. Soll das
  Telefon je senden dürfen, braucht es eine eigene Zustandsmeldung vom
  Server und eine Sicherung gegen Fehltipp.
* **Ein Symbol für den Startbildschirm.** Entwürfe liegen unter
  `docs/design/2026-09-30-handfunke-symbol.html`.
* **Zwei Zuhörer am selben Empfänger** teilen sich seit 2026-09-30 den Ton
  richtig (eigener Vorrat je Sitzung), aber `spectrum_start` für Empfänger 2
  wird abgelehnt — es gibt nur einen Spektrum-Abgriff.
* **Opus** würde den Ton von 13 auf 4 kB/s drücken. Serverseitig wäre es zu
  haben (libopus hängt wegen RADE ohnehin an der Verknüpfungszeile), im
  Browser nicht: WebCodecs' `AudioDecoder` ist ebenfalls [SecureContext] und
  fehlt über `http`. Bliebe eine WASM-Fremddatei — für 9 kB/s zu teuer.
