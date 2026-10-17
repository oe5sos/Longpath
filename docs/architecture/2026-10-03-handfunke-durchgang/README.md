# Handfunke — Durchgang vom 2026-10-03

Alles, was zwischen dem 2026-10-01 und dem 2026-10-03 an der Fernbedienung
gebaut wurde, einmal von vorne bis hinten nachgemessen. **Ohne Funkgerät und
ohne Antenne**, gegen `handfunke/attrappe.py` in echter Telefongröße
(375×812), Panadapter 373 px breit.

Das ist kein Prüfstand, der bei jedem Bau mitläuft — es ist ein Protokoll
dessen, was einmal nachgewiesen wurde, mit den Zahlen dazu. Wer eine dieser
Stellen ändert, soll hier nachlesen können, was sie können musste.

## Warum so gemessen wird

Zweimal an einem Vormittag wurde an der falschen Stelle gesucht, weil
„läuft es?" die falsche Frage war:

* „kein Ton" am SunSDR QRP — der Tonstrom lief mit **vollen 12 kB/s** und
  trug **Stille** (RMS −75 dBFS). Am Gerät hing keine Antenne.
* „ich sehe den Wasserfall, aber keinen Ton" — der Wasserfall lief, zeigte
  aber reines Rauschen: **7 dB** zwischen Rauschboden und stärkstem Punkt.

Darum misst dieser Durchgang **Pegel und Bildpunkte**, nicht Zustände.
Werkzeug dafür: `handfunke/pruefe.py`.

## Bedienung

| Was | Erwartet | Gemessen | |
|---|---|---|---|
| Feinschritt `+` / `−` | je Tipp genau 100 Hz | +100, +300 nach drei Tipps, −100 zurück | ✅ |
| Frequenz eintippen | sechs Schreibweisen | `7.134.600`→7134600 · `14074.5`→14074500 · `21.2155`→21215500 · `144300`→144300000 · `3.650`→3650000 · `145,500`→145500000 | ✅ |
| Blatt nach dem Setzen | schließt sich | geschlossen | ✅ |
| Zoomtasten | Raststufen auf und ab | 48→24→12→6 kHz, zurück 12→24 | ✅ |
| Zoom-Anschläge | klemmt unten und oben | 6 kHz bzw. 192 kHz, weitere Tipps wirkungslos | ✅ |
| Kneifen auseinander | kleinere Spanne | 24 → 6 kHz | ✅ |
| Kneifen zusammen | größere Spanne | 6 → 96 kHz | ✅ |

Das Kneifen konnte bis `d8c0e417` **rechnerisch nie** wirken: der
Bezugspunkt wurde bei jedem Ereignis nachgezogen, das Verhältnis lag immer
bei rund 1,001 und rastete auf denselben Wert ein.

## Anzeige

| Was | Erwartet | Gemessen | |
|---|---|---|---|
| Durchlassband USB (Filter 100…2500, 48 kHz auf 349 px) | links 175,2 px, breit 17,45 px | **175,23 px**, **17,45 px** | ✅ |
| Durchlassband LSB (gespiegelt) | links 156,3 px | **156,31 px**, gleiche Breite | ✅ |
| Ohne `dds:` vom Server | kein Band statt eines falschen | ausgeblendet | ✅ |
| Wasserfall bei +4 kHz (48 kHz Spanne) | um −31,1 px schieben, Inhalt behalten | Versatz **−31 px**, Übereinstimmung **0,995** | ✅ |
| Wasserfall bei +90 kHz (> Spanne) | Schnittmarke statt Schieben | bernsteinfarbene Marke auf y=2/3 (2 px), Altes wandert hinaus | ✅ |
| Rauschhinweis, Band mit Trägern | bleibt aus | `4 kB/s bild` | ✅ |
| Rauschhinweis, `attrappe.py --still` | erscheint in Warnfarbe | `nur rauschen (3 dB) — antenne?`, Klasse `warn` | ✅ |

## Sicherheit

| Was | Erwartet | Gemessen | |
|---|---|---|---|
| SENDEN und TUNE | sichtbar gesperrt | Klasse `tx locked` / `tune locked`, Beschriftung „NUR IN DER APP" | ✅ |
| Klick und langes Drücken (900 ms) auf beide | **kein einziger** Befehl an den Server | `[]` — nichts gesendet | ✅ |
| Quelltext | keine Stelle sendet `trx:` oder `tune:` | keine | ✅ |

Die Seite hat am 2026-09-30 einmal `tune:N,true` geschickt — ein
Dauerträger mit voller Leistung auf der Antenne, der nur nicht „senden"
heißt. Seither sind beide Tasten tot und sehen auch so aus. Diese drei
Zeilen sind der Grund, warum dieses Dokument existiert.

### Sendezustand anzeigen (2026-10-03 abends, PR #170)

Die Tasten bleiben tot — aber die Seite **meldet** jetzt, wenn die Station
sendet. `trx:` und `tune:` wurden seit dem ersten Tag mitgelesen und nirgends
verwendet: die Seite wusste es und sagte nichts. Für eine Fernbedienung, die
nur hört, ist das die unangenehmste Form von Stille — Wasserfall leer, Ton
weg, S-Meter unten, und nichts erklärt es.

Geprüft an der Attrappe (`--sendet 6` schaltet alle sechs Sekunden um; den
Zustand am echten Gerät herzustellen hieße senden, und das ist ohne Antenne
verboten).

| Was | Erwartet | Gemessen | |
|---|---|---|---|
| `trx:0,true` | Zeile in Messing statt grau | „STATION SENDET · EMPFÄNGER STUMM", Klasse `tx locked sendet` | ✅ |
| `trx:0,false` | zurück in Grau | „SENDEN NUR IN DER APP" | ✅ |
| Rauschhinweis beim Senden, Band ohne Träger | **unterdrückt** | Fußzeile nur `4 kB/s bild` | ✅ |
| derselbe Lauf, im Empfang | Hinweis kommt zurück | `nur rauschen (3 dB) — antenne?` | ✅ |
| Farbe | kein Rot | `--measured` (Messing), wie alles Gemessene | ✅ |

Die dritte und vierte Zeile gehören zusammen und sind der eigentliche Beleg:
auf einem Band ohne jede Station muss der Hinweis im Empfang **da** sein und
im Senden **weg**. Nur so ist gezeigt, dass er unterdrückt und nicht kaputt
ist.

Nicht angefasst: das S-Meter. Ob Longpath während des Sendens weiter echte
Empfangswerte meldet, ist nicht geprüft — und das zu prüfen hieße senden.
Ungeprüft etwas auszublenden wäre genauso geraten wie es ungeprüft
stehenzulassen.

## Verhalten, wenn der Server weg ist

Nachgestellt, indem die Attrappe mitten im Betrieb beendet und nach gut
zwanzig Sekunden neu gestartet wird. Das ist kein konstruierter Fall: die
Auto-Installation tauscht Longpath aus, während das Telefon verbunden ist —
am 2026-10-03 viermal.

| Was | Erwartet | Gemessen | |
|---|---|---|---|
| Kopfzeile | sagt sofort Bescheid | bei 8 s `NICHT VERBUNDEN`, Leuchte aus | ✅ |
| Bildrate in der Fußzeile | fällt auf null | `4 kB/s` → `1 kB/s` → leer binnen 2 s | ✅ |
| Kopplungsblatt | kommt nach der Schonfrist | bei 29 s (12 s Frist) | ✅ |
| Wiederverbinden | von selbst | bei 46–47 s, ohne Zutun | ✅ |
| Kopplungsblatt danach | geht wieder zu | geht zu (`50b8696a`) | ✅ |
| S-Meter bei Abriss | zeigt nichts mehr | `—`, Balken 0 % (`4d866388`) | ✅ |
| Betriebsart / Band / Filter | bleiben stehen, bewegen sich aber nicht auf Tippen | unverändert bei totem Draht | ✅ |
| Leistungsregler | bewegt sich nicht ohne Server | unverändert | ✅ |
| **Abriss mitten im Senden** | Sendezeile fällt zurück, bleibt nicht stehen | „SENDEN NUR IN DER APP", Fußzeile `◇ getrennt`, S-Meter `—`, Panadapter „warte auf Longpath…" | ✅ |

Die letzte Zeile ist die Zerstörprobe für den neuen Sendezustand: die
Attrappe wurde abgeschossen, **während** sie `trx:0,true` gemeldet hatte. Ein
eingefrorenes „STATION SENDET" wäre genau der Fehler gewesen, gegen den die
Anzeige gebaut ist — und kein Prüfstand hätte ihn gezeigt, weil ich ihn selbst
geschrieben hätte.

Die zwei davor sind **kein** Mangel, sondern die richtige Unterscheidung:
ein eingefrorener **Messwert** lügt, eine eingefrorene **Einstellung** nicht.
Betriebsart und Filter ändern sich nicht von selbst; das S-Meter schon. Und
weil die Seite ihren Zustand ausschließlich vom Server übernimmt, gibt ein
Tipp ins Leere auch keine falsche Rückmeldung.

### Was dabei gefunden wurde

Zwei Anzeigen logen nach einem Abriss weiter, beide in `#157` behoben:

* Das **Kopplungsblatt** blieb über einer längst wiederhergestellten
  Verbindung liegen.
* Das **S-Meter** hielt dreißig Sekunden lang „S7 · −82 dBm", weil sein
  Maßstab der Socket war statt `ready`.

Dasselbe Muster wie an zwei anderen Stellen am selben Tag (schlafender
AudioContext, bunter Wasserfall ohne Antenne): **die Oberfläche sah nicht so
aus, wie der Zustand war.** Die Regel, die alle drei Fälle löst, steht schon
beim Durchlassband — lieber nichts zeigen als etwas Falsches.

## Was hier NICHT geprüft werden kann

* **Ton am Lautsprecher.** Ob iOS die Wiedergabe-Sitzung annimmt, zeigt nur
  das Gerät — mit Stummschalter an und ohne Kopfhörer. Erkennbar an
  `sitzung=playback` in der Selbstmeldung (`/melde`).
* **Die Drahtmeldung `spectrum_span`.** Dafür braucht es eine FFTEngine,
  also ein RadioModel; die Attrappe schickt sie nicht. Beim Rückfallweg
  (keine Meldung → eigener Wunsch gilt) läuft dieser Durchgang mit.
* **Verhalten am echten Band.** Hören, Abstimmen im Gehen, Treffsicherheit
  der Schieber. Das entscheidet das Ohr, nicht die Messung.

## Fallstrick für den nächsten Durchgang

Ein über `javascript_tool` abgebrochenes Skript läuft in der **Seite
weiter**. Am 2026-10-03 klickte eine so verwaiste Schleife alle 150 ms auf
„herauszoomen" und machte jeden Zoom sofort zunichte — das sah nach einem
Fehler der Handfunke aus und war keiner. Erkennbar daran, dass die
Klickereignisse `isTrusted: false` tragen.

**Nur begrenzte Schleifen in Seitenskripten, und im Zweifel die Seite neu
laden, bevor gemessen wird.**

Zweiter Fallstrick, derselbe Tag: ist der Browser-Bereich **verborgen**,
pausiert `requestAnimationFrame` — und damit die ganze Zeichenschleife. Dann
stehen Datenraten und Fußzeile still, während Ton, S-Meter und
Frequenzanzeige weiterlaufen (die hängen an Ereignissen, nicht am Bild). Das
sieht aus wie eine tote Fußzeile und ist keine:

```
versteckt=true   fuss=""            rate=""
nach dem Nach-vorne-Holen:
versteckt=true   fuss="4 kB/s bild" rate="15 kB/s"
```

Auf dem Telefon ist dieses Verhalten richtig — im Hintergrund soll nicht
gezeichnet werden, und der Ton läuft unabhängig weiter. Beim Messen muss man
es nur wissen: **vor jeder Messung an der Bildschleife den Tab nach vorne
holen** (`document.hidden` prüfen).
