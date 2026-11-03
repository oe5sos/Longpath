# Der TCI-Spektrumstrom hing an der falschen Quelle

2026-11-03. Betrifft `main` bis einschließlich #376.

## Der Befund, gemessen

Am 2026-11-02 am echten Gerät: Longpath an der Flex, 40 m, lebendes
Bild im eigenen Panadapter. Ein nackter Prüf-Client (nur
WebSocket-Handschlag und `spectrum_start:0,256,10;`, zählt Binärrahmen)
bekommt:

| Was | Ergebnis |
| --- | --- |
| Handschlag | 101, Accept stimmt |
| Textfolge (`protocol:`, `device:`, …) | kommt vollständig |
| Bestätigung `spectrum_start:0,256,10;` | kommt |
| **Binärrahmen in 8 s** | **0** |
| Ausgehende Rate laut `nettop` | ~19 B/s statt ~7 kB/s (718 Punkte × 10 Bilder) |

Und das Protokoll von Longpath meldete dabei Erfolg:

```
INF: TciServer: Spektrum-Abgriff an FFTEngine::fftReady
INF: TciServer: Spektrum abonniert rx 0 718 Punkte, 10 B/s, peer ...
```

Beide Zeilen sind wahr. Beide melden eine Absicht. Keine meldet, dass
je ein Bild angekommen wäre. **Verbunden ist nicht erreicht.**

Dasselbe Protokoll (`longpath-20261102-133528.log`) zeigt vier Minuten
davor, woher das Bild in Longpath wirklich kam:

```
[13:36:42] INF: Flex-Bild: 4096 Bins, Ausschnitt 6.98534–7.18534 MHz; …
[13:40:20] INF: TciServer: Spektrum-Abgriff an FFTEngine::fftReady
[13:40:20] INF: TciServer: Spektrum abonniert rx 0 718 Punkte, 10 B/s, peer …
```

Das Bild kam als `Flex-Bild`, der Abgriff hing an der FFTEngine. Zwei
verschiedene Leitungen.

## Die Ursache

`TciServer::ensureFftTap()` hängte `onFftBinsReady` an
`m_model->fftEngine()`. Das ist die FFTEngine von **Strom 0** —
`MainWindow::setFftEngine(primaryFftEngine())`, genau einmal gesetzt.
Diese Engine rechnet, wenn ein OpenHPSDR-Gerät seine rohen I/Q-Daten
auf Strom 0 liefert. Sonst nicht.

Drei Geräte, drei Wege, ein Abgriff:

| Gerät | Wo das Spektrum entsteht | Feuert `fftReady` von Engine 0? |
| --- | --- | --- |
| ANAN / HL2 (OpenHPSDR) | `FFTEngine(0)`, gefüttert aus `rawIqDataForStream` | ja |
| Flex | in der **Maschine**; kommt fertig als `FlexVitaStream::spectrumReady` → `RadioModel::flexSpectrumReady` | **nie** — die Engine wird nicht gefüttert |
| SunSDR2 QRP | `FFTEngine(100000)`, Pseudo-Stromnummer aus `MainWindow_SunSdr.cpp` | **nie** — eine andere Engine; und ihre Empfängernummer 100000 hätte `onFftBinsReady` ohnehin verworfen (`receiverId > 1`) |

Die Flex-Seite hatte denselben Fehler schon einmal, eine Stufe weiter
vorne: Clarity bekam seine Bins „AUSSCHLIESZLICH aus `primaryFftEngine()`"
und stand an der Flex ohne Boden (MainWindow_Flex.cpp, 2026-10-22, der
einfarbig rote Wasserfall). Dort wurde Clarity einzeln nachverdrahtet.
Der TCI-Strom war der nächste Abnehmer an derselben toten Leitung.

## Die Behebung

Ein **geräteunabhängiger Weg** am Modell, den jede Quelle füttert:

```
RadioModel::meldeSpektrum(rx, dbm, spanneHz)  →  signal spektrumBereit(rx, dbm, spanneHz)
```

| Quelle | füttert in |
| --- | --- |
| `FFTEngine(0)`, `FFTEngine(1)` | `MainWindow::createFftEngineForStream` → `verbindeSpektrumMeldung(engine, streamIndex)` |
| SunSDR-Engine (100000) | `MainWindow_SunSdr.cpp` → `verbindeSpektrumMeldung(engine, 0)` |
| Flex | `RadioModel::startFlexStreams`, im selben Lambda wie `flexSpectrumReady` |

`spanneHz` sagt, wie viele Hertz die Bins zusammen abdecken: die
Abtastrate der Engine, der Panadapter-Ausschnitt der Maschine bei der
Flex (`m_flexAusschnittHochHz − m_flexAusschnittTiefHz`, **nicht** die
Breite der Wasserfallkachel, die ihren eigenen Rand hat). Der Server
nimmt damit den Ausschnitt und meldet `spectrum_span:` richtig, auch
wenn nie ein `iq_samplerate` vom Gerät kam — die Flex schickt keines.

Der Server hängt sich in `start()` ans Modell, nicht mehr nachträglich
an eine Engine. Das Modell gibt es beim Serverstart; die Engine gab es
nicht, und daraus war die ganze `ensureFftTap`-Nachholerei entstanden.

Fäden: `fftReady` kommt vom FFT-Faden, `meldeSpektrum` wird dort gerufen
(Kontext ist die Engine, kein Umweg über die Hauptschleife je Bild), das
Signal geht von dort ab; der Server hängt mit `AutoConnection` und
bekommt es in seinem Faden. Die Flex meldet aus dem Hauptfaden, dann ist
es ein direkter Aufruf. `QPointer` auf das Modell fängt den Abbau.

Und eine Zeile, die **Ankunft** meldet statt Absicht, einmal je Sitzung:

```
INF: TciServer: erstes Spektrumbild gesendet, rx 0 4096 Bins -> 256 Punkte, Spanne 200000 Hz, peer ...
```

Fehlt sie bei stehendem Abonnement, ist die Quelle tot — nicht der Client.

## Der Prüfstand

`tests/tst_tci_spektrum_quelle.cpp` fährt den Weg Modell → Server →
echter `QWebSocket`-Client:

| Fall | hält fest | alte Fassung |
| --- | --- | --- |
| `modell_meldung_erreicht_den_client` | Abonnement, ≥ 5 Rahmen, Stromtyp 100, 256 Punkte, Träger von Bin 2048/4096 auf Punkt 128 (Byte 140), Rauschen Byte 80, `spectrum_span:0,48000;` aus der Meldung | **rot, 0 Rahmen** |
| `flex_meldung_erreicht_den_client` | der Flex-Einstieg `meldeFlexSpektrum` selbst, Ausschnitt 6,985–7,185 MHz wie im Gerätelog, 718 Punkte wie das Logprogramm: Fenster bekommt `flexSpectrumReady` weiterhin, Client ≥ 5 Rahmen, Abtastart 100, Träger auf Punkt 359, `spectrum_span:0,200000;` | **rot, 0 Rahmen** |
| `spanne_der_quelle_bestimmt_den_ausschnitt` | 200 kHz gemeldet, 50 kHz verlangt → mittig geviertelt, `spectrum_span:0,50000;`, Träger bleibt in der Mitte | **rot, 0 Rahmen** |
| `fremde_empfaengernummer_wird_verworfen` | rx 100000 liefert nichts — darum muss die SunSDR-Engine als 0 angehängt werden | grün (verwirft auch alt) |
| `ohne_abonnement_kein_rahmen` | — | grün |

**Gegenprobe gegen die alte Fassung**, gefahren am 2026-11-03:
`TciServer.{h,cpp}` und `TciClientSession.h` von `origin/main`
zurückgespielt, Modell und Prüfstand neu. Ergebnis: die drei Fälle, in
denen ein Bild zugestellt werden soll, fallen mit **0 Rahmen** um
(`Totals: 4 passed, 3 failed`, 13,8 s) — derselbe Befund wie am Gerät,
auf allen drei Wegen. Mit der Behebung: 7/7 grün in 3,6 s.

## Am Gerät, 2026-11-03 17:26

Longpath 0.8.2 aus diesem Zweig (`3ad62e9ba`), vom Betreiber gestartet,
an der Flex auf 20 m. Derselbe Prüfling wie am Vortag:

```
python3 -I tci_probe.py 50001 256 8
Handschlag: HTTP/1.1 101 Switching Protocols | Accept stimmt: True
>> spectrum_start:0,256,10;
   erster Rahmen: 320 Byte, Stromtyp=100, Abtastart=100, Punkte=256
<< Binaerrahmen in 8s: 65
```

| | 2026-11-02 (alt) | 2026-11-03 (neu) |
| --- | --- | --- |
| Binärrahmen in 8 s | 0 | **65** |

Und im Protokoll die Zeile, die es vorher nicht gab:

```
[17:26:38.386] INF: TciServer: Spektrum abonniert rx 0 256 Punkte, 10 B/s, peer …
[17:26:38.429] INF: TciServer: erstes Spektrumbild gesendet, rx 0 4096 Bins -> 256 Punkte, Spanne 200000 Hz, peer …
```

43 ms vom Abonnement bis zum ersten Bild; die Spanne ist der
200-kHz-Ausschnitt der Maschine. 65 statt der rechnerischen 80 Rahmen:
die Drossel misst gegen die Ankunft der Flex-Bilder, nicht gegen eine
Uhr, und der Rückstau-Schutz lässt kein Bild warten — das ist gewollt.

## Was sich daraus merken lässt

1. **Ein Abgriff an „der" Engine ist ein Abgriff an Strom 0.** Longpath
   hat drei Quellen für ein Spektrum und zwei davon gehen an Engine 0
   vorbei. Wer ein Bild braucht, hängt ans Modell.
2. **Protokollzeilen, die Absicht melden, beweisen nichts.** „Abgriff
   verbunden" und „abonniert" standen da, während 0 Rahmen gingen. Die
   Zeile, die zählt, ist die beim ersten gesendeten Rahmen.
3. **Der Client war unschuldig, und das war vorher belegt.** Die
   Contestprogramm-Seite war gegen ein Doppel mit 400 Rahmen geprüft; der
   nackte Prüfling hat die Suche dann auf eine Seite eingegrenzt, bevor
   irgendwo geändert wurde.
