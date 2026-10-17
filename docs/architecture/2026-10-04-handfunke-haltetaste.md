# Die Halteleiste — der Sendeton ohne Sendeweg

**2026-10-04**

## Was das ist

Entwurf 1 von vier wurde gewählt (`docs/design/2026-10-04-sendetaste/`):
**gedrückt halten nimmt auf, loslassen hört auf.** Der Finger ist die
Sicherung — fällt das Telefon aus der Hand, endet es von selbst, und es
bleibt kein Zustand übrig, der hängenbleiben könnte.

Gebaut ist davon alles **außer dem Tasten**. Es gibt in diesem Zweig kein
`trx:` und kein `tune:`, und das bleibt so, bis der Betreiber es
ausdrücklich freigibt und die Dummy-Last dranhängt. Dieselbe Reihenfolge wie
bei `tx-ton.js`: erst alles, was nicht tasten kann.

## Warum das trotzdem etwas wert ist

Zwei Dinge, die heute schon zählen:

**Der Pegel.** Ohne ihn weiß am Telefon niemand, ob das Mikrofon überhaupt
etwas hört. Das ist die erste Frage, lange bevor jemand tastet — und sie
wäre sonst erst auf der Luft zu beantworten, am teuersten möglichen Ort.

**Der Beleg, dass die Kette steht.** Longpath nimmt TX-Ton nur von dem
Client an, der *gerade tastet* (`m_txAudioActiveClient`, TciServer.cpp:3634),
und zählt alles andere als verworfen (`session->txFramesDropped`). Dieser
Zähler ist der Nachweis, dass Mikrofon → Ratenwandlung → mu-law → Rahmen →
Netz → Server vollständig funktioniert, **ohne dass ein Watt die Antenne
sieht**. Eine Prüfung, die nicht senden muss.

## Die Trennung in der Datei

`handfunke/mikrofon.js` hat zwei Teile, und die Trennung ist der Punkt:

* **`Sendestrecke`** — reine Rechnung. Proben hinein, Rahmen heraus. Kein
  Browser, kein Mikrofon, kein Netz. Dort sitzt alles, was still falsch
  rechnen könnte, und genau das ist ohne Gerät prüfbar.
* **`Mikrofon`** — die Anbindung an den Browser. Dünn gehalten, weil sie
  sich nicht prüfen lässt.

### Entscheidungen, die nicht offensichtlich sind

**48 kHz, mu-law, einkanalig.** WDSPs TXA-Eingang läuft mit 48 kHz; wer
etwas anderes schickt, zwingt den Server, einen Umsetzer anzulegen. Das
Telefon liefert ohnehin meist 48 kHz — dann ist die Wandlung hier ein
Nullschritt und im Server gar keiner. mu-law halbiert gegenüber Int16 und
war im Empfangsweg mit 38,4 dB gemessen; für Sprache reichlich.

**20 ms je Block.** 50 Rahmen je Sekunde: fein genug, dass die Verzögerung
nicht auffällt, grob genug, dass der 64-Byte-Kopf neben 960 Nutzbytes nicht
ins Gewicht fällt.

**Alle drei Aufbereitungen aus** — `echoCancellation`, `noiseSuppression`,
`autoGainControl`. Sie sind für Telefonie gebaut und auf einer Funkstrecke
schädlich: die Echounterdrückung schneidet Silben weg, sobald der Empfänger
mithört, die Rauschsperre frisst leise Sprache, und die Verstärkungsregelung
pumpt gegen die Sprachaufbereitung im Funkgerät. Was geregelt wird, wird
dort geregelt.

**Kein Messing.** Messing ist die Sendefarbe. Solange hier nichts getastet
wird, wäre Messing eine Behauptung. Die Farbe kommt mit dem Tasten.

**Der Finger kann vor der Antwort weg sein.** `getUserMedia` fragt beim
ersten Mal nach Erlaubnis und braucht dann Sekunden. Wird in dieser Zeit
losgelassen, hört die Aufnahme sofort wieder auf, statt stumm weiterzulaufen.

**Fenster in den Hintergrund = loslassen.** Dort ist niemand mehr, der
loslässt. Ein Mikrofon, das im Hintergrund weiterläuft, wäre auf einer
Fernbedienung das Letzte, was jemand erwartet.

## Geprüft

`handfunke/pruefe-mikrofon.mjs`, siebzehn Punkte, ohne Browser und ohne
Gerät. Die beiden, die zählen:

**Kein Rest geht verloren.** Krumme Blockgrößen, wie ein Telefon sie
liefert (128, 2048, 333, 4096, 777, 1024) — alles bis auf den letzten
Teilblock muss herauskommen. Der Fehler dahinter hört sich nicht an wie ein
Fehler, sondern wie ein schlechtes Mikrofon.

**Keine Stufe an der Blockgrenze.** Ohne den übertragenen Rest fängt jeder
Block wieder bei Probe 0 an; bei 50 Blöcken je Sekunde hört man das als
Rauen. Die Schwelle ist **gemessen, nicht geschätzt**: gegen eine Fassung
ohne Rest liegt die größte Stufe durchweg beim 1,5-fachen — bei 60, 400,
1200 und 2400 Hz gleichermaßen. Bei 400 Hz:

| | größte Stufe |
|---|---|
| ideal (glatter Sinus) | 0,0262 |
| mit übertragenem Rest | 0,0262 |
| **ohne** | **0,0407** |

`1,25 × ideal = 0,0328` trennt beides mit rund einem Viertel Luft nach jeder
Seite. Ein erster Versuch mit `1,5 ×` ließ 0,0407 gegen 0,0393 stehen — ein
Fänger mit so wenig Luft fängt beim nächsten Mal nichts mehr.

**Gegenproben gefahren**, beide greifen:

* Rest nicht übertragen → `FEHLT keine Stufe an der Blockgrenze`
* Vorrat wächst nicht mit → `RangeError: offset is out of bounds`

## Nicht geprüft

**Das Telefon.** Dort gibt es bis zum Zertifikat (#188) überhaupt kein
Mikrofon — `navigator.mediaDevices` ist `undefined`, gemessen über die
Netzadresse. Die Seite sagt das jetzt auch, statt eine Taste anzubieten, die
beim Drücken nichts tut: „KEIN MIKROFON ÜBER HTTP — ZERTIFIKAT FEHLT".

**Wie der Sendeton klingt.** Das entscheidet Martins Ohr, nicht die Zahlen,
und es entscheidet sich erst beim ersten echten Tasten — an der Dummy-Last.
