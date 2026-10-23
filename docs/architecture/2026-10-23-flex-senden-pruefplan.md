# Senden über die FLEX-8400 — Stufe 2 und ihr Prüfplan

*2026-10-23. Gebaut, im Prüfstand belegt, **am Gerät noch nicht
getastet**.*

## Was Stufe 1 nicht tat

Im Kopf von `src/core/FlexRadioConnection.h` stand seit dem 2026-10-18
ausdrücklich:

> Was Stufe 1 trotzdem NICHT tut: senden. Kein `xmit`, keine
> TX-Scheibe, keine Leistung. Die Interlock-Meldung der Maschine
> (`state=RECEIVE tx_allowed=0`) wird gelesen, nie geschrieben.

Der Betreiber hat am 2026-10-23 danach gefragt. Dies ist Stufe 2.

## Der Unterschied zu einem OpenHPSDR-Gerät

Bei ANAN/Hermes rechnet **Longpath** das Sendesignal: Mikrofon → WDSP
(`TxWorkerThread`, `TxChannel`) → I/Q auf die Leitung. Die Flex rechnet
**selbst**. Es geht also kein fertiges Signal hinaus, sondern roher
Mikrofonton; die Maschine macht daraus SSB.

Darum hängt der Flex-Sendeweg an **keinem** Stück des WDSP-Sendepfads.
Er zieht direkt an `AudioEngine::pullTxMic` und schickt VITA-49-Pakete.

## Die Teile

| Stück | Wo |
| --- | --- |
| Befehle (`xmit`, `slice set tx=`, `transmit set rfpower=`/`tunepower=`, `transmit tune`, `transmit set dax=`, `stream create type=dax_tx`) | `FlexCommandWire` in `src/core/FlexRadioConnection.{h,cpp}` |
| Tonpaketbau (VITA-49, Pakettyp 1) | `FlexVita::buildTxAudioPacket` in `src/core/FlexVitaReader.{h,cpp}` |
| Pakete hinaus (UDP 4991) | `FlexVitaStream::sendeTxPaket` |
| Tor und Zustand (Verriegelung) | `RadioModel::verarbeiteFlexVerriegelung`, `flexDarfSenden()`, `flexSendetGerade()` |
| Tasten, Leistung, Abstimmen, Tonstrom | `RadioModel::flexTasten` / `setzeFlexSendeleistung` / `flexAbstimmen` / `legeFlexSendetonstromAn` / `sendeFlexMikrofonton` |
| MOX, TUNE, Regler, Mikrofonpumpe | `MainWindow::verdrahteFlexSenden` in `src/gui/MainWindow_Flex.cpp` |

## Drei Fallen, die man nicht raten darf

1. **Ohne Sendescheibe weist die Maschine `xmit` ab.** `slice set <n>
   tx=1` wird beim Verbinden gesetzt.
2. **Ohne `transmit set dax=1` verfällt jedes Tonpaket.** Die Maschine
   nimmt dann das Mikrofon an ihrer eigenen Buchse und wirft unsere
   Pakete weg — getastet wird, gesendet wird Stille. Longpath setzt
   `dax=1` erst beim Tasten, nicht beim Verbinden: sonst nähme es dem
   Betreiber das Mikrofon an der Maschine weg. (AetherSDR
   `AudioEngine.cpp:8954` [@d58e2b8a] sagt dasselbe, von der anderen
   Seite: `dax=0` nicht beim Lösen schicken, der Zustandsschub kreuzt
   es.)
3. **Der Sendeton geht an UDP 4991**, nicht an 4992. 4992 ist nur die
   Anmeldung, aus der die Maschine unsere Adresse lernt.

Dazu: der Sendekopf ist **nicht** der Empfangskopf. Hinaus geht
Pakettyp 1 (`IFDataWithStream`) mit sieben Kopfwörtern, herein kommt
Pakettyp 3 (`ExtDataWithStream`) mit 28 Byte Kopf. Ein Prüfstand nagelt
das fest, damit niemand den Leser spiegelt.

## Das Tor

`flexTasten(true)` geht nur hinaus, wenn **alle drei** stehen:
Verbindung, Sendescheibe, und `tx_allowed=1` von der Maschine. Jedes
fehlende Stück wird **einzeln benannt** — „es sendet nicht" ohne Grund
hat am 2026-10-22 einen Abend gekostet.

`flexTasten(false)` läuft durch **kein** Tor. Aufhören zu senden muss
immer gehen.

Die Vorgabe von `flexDarfSenden()` ist **false**: solange keine
Verriegelungsmeldung kam, wird nicht getastet.

## Was die Prüfstände belegen

`tests/tst_flex_senden.cpp`, 17 Fälle, ohne Funkgerät:

- der Wortlaut jedes Befehls, einschließlich der Klemmung 0..100 (außerhalb wird **gar nichts** geschickt — ein abgewiesener Befehl ließe die Leistung auf dem alten Wert stehen, und der Betreiber glaubte, er habe heruntergeregelt);
- der Sendekopf Feld für Feld gegen AetherSDR;
- der Ton kommt unverändert im Rumpf an (Sinus hinein, Sinus heraus);
- der Rollzähler läuft und rollt bei 16 um — und läuft bei einem verworfenen Paket **nicht** weiter, sonst risse es eine Lücke in die Folge;
- die drei Formen, in denen die Antwort auf `stream create` ihre Kennung trägt;
- das Tor: ohne Verbindung kein `xmit`, kein Abstimmträger;
- die Verriegelung wird gelesen, samt Quelle (`MIC` = Taste am Gerät, `SW` = unser `xmit`) und Grund (`AMP:TG`).

## Was am Gerät noch zu fahren ist

**Nichts davon ohne den Betreiber.** Er hat eine ACOM 06AT in der
Leitung; jeder Sendeversuch wird vorher abgesprochen, und die ersten
gegen einen 50-Ohm-Abschluss oder mit kleiner Leistung.

| # | Schritt | Erwartung |
| --- | --- | --- |
| 1 | Verbinden, Protokoll lesen | `Flex: Scheibe N ist jetzt die Sendescheibe`, `Flex: Sendetonstrom 0x…`, `Flex-Senden verdrahtet — es wurde nichts getastet`; **keine** Verriegelungsmeldung mit `tx_allowed=1` erzwungen |
| 2 | Taste am Mikrofon der Maschine drücken (ohne Longpath) | `Flex SENDET (nicht von uns)`, Mikrofonpumpe läuft **nicht** |
| 3 | Leistung in Longpath auf 1 W stellen | `transmit set rfpower=1` im Protokoll, SmartSDR/AetherSDR zeigt 1 W |
| 4 | **Mit 50-Ohm-Abschluss**: TUNE in Longpath | Abstimmträger, `tunepower`, kein Mikrofonton nötig |
| 5 | **Mit Abschluss**: MOX, in das Mikrofon sprechen | `transmit set dax=1` vor `xmit 1`; die Maschine zeigt Modulation; `FlexVitaStream::txPaketeGesendet()` wächst |
| 6 | MOX lösen | `xmit 0`, Pumpe steht, Verriegelung zurück auf `RECEIVE` |
| 7 | Gegenprobe: MOX bei gesperrter Maschine | kein `xmit`, Grund im Protokoll, MOX springt zurück |
| 8 | Erst danach, nach ausdrücklicher Freigabe: an der Antenne, kleine Leistung, Rapport von einem Gegenüber oder einem Netzempfänger | verständliche Sprache |

## Was Stufe 2 weiterhin nicht tut

CW tasten, PureSignal, Mithören über die Maschine, Sendeton­bearbeitung
in Longpath (die Flex hat ihre eigene). Und sie tastet nie von selbst —
jede Taste kommt vom Betreiber.
