# Werkbank: Longpath gegen den HPSDR-Simulator

Der Betreiber hat einen ANAN-10E und eine ANVELINA, aber keinen Hermes-Lite 2
— und Longpath soll „schluss endlich für hermes auch funktionieren"
(2026-09-21). Ohne Geraet bleibt ein Simulator: `hpsdrsim` von
Christoph van Wüllen, DL1YCF, aus pihpsdr/deskhpsdr (GPL). Er spielt auf
Wunsch Hermes, Hermes-Lite 1/2, Angelia, Orion, Saturn/G2 … und schreibt
jeden Steuerwert, den er ueber Protokoll 1 oder 2 bekommt, auf die Konsole
(RX FREQ, SampleRate, PTT, TX DRIVE, HL2-Bits, ATT). Genau das ist der Teil,
den kein Signal-Spy im Pruefstand sieht.

Der Simulator ist **Messgeraet, keine Quelle**: nichts davon wandert in den
Longpath-Baum. Die Werkbank-Pruefstaende sind Longpath-eigen.

## Simulator bauen

Quelle: `deskhpsdr/src/hpsdrsim.c`, `newhpsdrsim.c`, `hpsdrsim.h`, `MacOS.h`
(Klon liegt unter `~/Longpath/deskhpsdr`). Vier Stellen in `hpsdrsim.c`
sind fuer die Werkbank noetig (gebaute Fassung liegt unter
`~/Longpath/hpsdrsim-bench`):

```c
setvbuf(stdout, NULL, _IOLBF, 0);                 /* main(): Zeilenpuffer, sonst
                                                     kommt das Protokoll erst beim Ende */
addr_udp.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* nur 127.0.0.1 — das Ding
                                                     antwortet sonst im ganzen LAN
                                                     auf jede Discovery */
/* Hauptschleife: den TCP-Weg stilllegen — Longpath spricht nur UDP.   */
if (0 && sock_TCP_Client < 0 && udp_retries > 10 && ODEVICE != DEV_NONE) {
/* … und einen recvfrom-Fehler melden statt beenden:                     */
if (bytes_read < 0 && errno != EAGAIN) { t_perror("recvfrom"); continue; }
```

Und, seit dem 2026-09-22, ein fuenfter Patch in `hpsdrsim.h` — der
wichtigste, weil er den „Wedge" aus Runde 2 endgueltig erklaert:

```c
#define WB_TXPAD 8192
EXTERN double  isample[OLDRTXLEN + WB_TXPAD];   /* war [OLDRTXLEN] */
EXTERN double  qsample[OLDRTXLEN + WB_TXPAD];
```

Die Hauptschleife schreibt ein ganzes Sendepaket (bis zu ~1000 Werte) in
`isample`/`qsample` und prueft den Umbruch **erst danach**
(`if (txptr >= OLDRTXLEN) { txptr = 0; }`). Am Pufferende laeuft sie also
ueber — und traf die naechsten Globals: `sock_udp` stand danach auf **0**,
jedes `recvfrom()` lieferte ENOTSOCK, und der Simulator drehte mit 100 %
CPU im Kreis (mit dem `continue`-Patch) oder beendete sich (ohne). Genau
dieselbe Klasse wie der WDSP-Eingangsring vom 21.09.: eine Schranke, die
nur *hinterher* prueft. Das Polster faengt den Ueberlauf, die Logik bleibt
Wort fuer Wort die des Originals. Seither laufen Ratenwechsel durch.

Ohne die letzten beiden lief der Simulator auf macOS nach dem
STOP/START eines Ratenwechsels zweimal in seinen TCP-Zweig (einmal
100 % CPU ohne UDP-Verarbeitung, einmal Abbruch mit „recvfrom: Socket
operation on non-socket") — Longpath sah dann eine stumme ep6-Seite,
meldete „LAN PHY throttle", LinkLost und drei vergebliche Reconnects.
Das ist der Simulator, nicht Longpath: mit stillgelegtem TCP-Zweig
liefen fuenf Laeufe hintereinander sauber durch.

Bauen (macOS, ohne `MacOS.c`):

```bash
cc -O2 -Wno-everything -o hpsdrsim hpsdrsim.c newhpsdrsim.c -lpthread -lm
```

Starten, z. B. als Hermes-Lite 2 (weitere: `-hermes`, `-hermeslite`,
`-angelia`, `-orion2`, `-g2`, dazu `-anan10e` fuer den 10E-Zuschnitt):

```bash
./hpsdrsim -hermeslite2 < /dev/null > sim.log 2>&1 &
```

**`< /dev/null` ist Pflicht.** Der Simulator liest in seiner Hauptschleife
die Tastatur (`select` auf stdin); mit einem geerbten, halb offenen stdin
liest er in jeder Runde ins Leere. Und: **immer nur ein Simulator** —
ein zweiter scheitert am TCP-Bind („Address already in use"), beendet
sich, und die Werkbank spricht unbemerkt mit dem alten (anderes Board,
anderes Protokoll).

Port 1024/UDP. `-hermes2` trifft in der Optionsliste zuerst auf `-hermes`
und liefert deshalb ebenfalls den Hermes.

## Werkbank fahren

Beide Pruefstaende **ueberspringen sich selbst** (`QSKIP`), solange
`LONGPATH_HPSDRSIM` nicht gesetzt ist — die CI hat keinen Simulator, und ein
Test, der einen braucht und keinen findet, wuerde dort still scheitern.

```bash
cmake --build build --target tst_hpsdr_sim_workbench tst_hpsdr_sim_gui_workbench

# Modell allein: Discovery → Verbinden → Empfangsstrom → Frequenz →
# S9-Referenzton (-73 dBm auf 14,100 MHz) → Daempfungsglied +10 dB →
# Abtastrate → TUNE (Vorwaertsleistung, RadioStatus) → TUNE ueber den
# Tune-Regler → MOX → zweiter Empfaenger → Trennen; danach liest er sim.log
# und prueft, dass 14,2 MHz, 7,05 MHz (RX 2), die neue Rate, die Daempfung
# und PTT=1 ankamen und der LETZTE PTT-Stand 0 ist.
LONGPATH_HPSDRSIM=127.0.0.1:1024 LONGPATH_HPSDRSIM_LOG=$PWD/sim.log \
  QT_QPA_PLATFORM=offscreen ./build/tests/tst_hpsdr_sim_workbench

# Mit echtem MainWindow: neun Bildschirmfotos (vor, verbunden, 40 m,
# Setup → Hardware samt HL2-Reiter, TUNE mit offener Diagnoseseite
# „Radio Status", getrennt) nach LONGPATH_GRAB_DIR. GPU-Flaechen werden per
# grabFramebuffer() eingelesen, deshalb OHNE offscreen — die Fenster
# erscheinen kurz auf dem Schirm.
LONGPATH_HPSDRSIM=127.0.0.1:1024 LONGPATH_GRAB_DIR=/tmp/grab \
  ./build/tests/tst_hpsdr_sim_gui_workbench
```

`LONGPATH_HPSDRSIM_NO_TX=1` laesst die TUNE-Station aus. Die Einstellungen
der Pruefstaende liegen wie bei allen Tests im `qttest`-Sandkasten
(`TestSandboxInit.cpp`), nie in den echten. Der erste Lauf je Sandkasten
dauert ~3 Minuten (FFTW-Wisdom), danach Sekunden.

## Was die Werkbank am 2026-09-21 gefunden hat

- Hermes, Hermes-Lite 1 und Hermes-Lite 2: Discovery, Verbindung, Empfang,
  Abstimmen, Ratenwechsel (192 → 384 kHz am HL2, 192 → 96 kHz am Hermes),
  TUNE (PTT=1) und Trennen (PTT=0) laufen durch; die Oberflaeche zeigt
  „Hermes Lite 2 · HL2 · v73", PA-Temperatur, PC-Mikrofon.
- `RadioModel::setConnectionState()` loeschte die in `connectToRadio()`
  gespeicherte Abtastrate und Empfaengerzahl bei JEDEM Zustand ausser
  Connected — also auch bei Probing/Connecting, die eine Ereignisrunde
  spaeter eintreffen. Ergebnis: Rate 0 und `rx2Enabled()==false` fuer die
  ganze Sitzung, bis der Betreiber die Rate von Hand wechselte
  (Netzwerkdiagnose „—", TCI `iq_samplerate` aus dem Cache). Behoben,
  Regression in `tst_sample_rate_live_apply`.
- Das Filterbild im RX-Applet bekam die VFO-Frequenz nur beim Binden der
  Scheibe; nach dem Abstimmen stand die Achse weiter auf 14.221…14.229.
  Dasselbe Loch wie am 2026-08-22 im Bandfilter-Applet. Behoben, Regression
  in `tst_filter_follows_the_panadapter`.

Nicht bewertet (nur notiert): der HL2 sieht beim Verbinden `TX DRIVE=255`
und `PA enable=1` bei PTT=0 — beides Absicht laut `P1CodecHl2.cpp` (der
mi0bot-Weg, ohne das Bit flattert das T/R-Relais bei TUNE), die 255 ist die
100-%-Stellung des Sandkastens.

## Runde 2 (2026-09-21, „hl2"): Empfang, Pegel, Senden, Diagnose

Was der Simulator hergibt und die Werkbank jetzt prueft:

- **S9-Referenzton.** hpsdrsim legt -73 dBm auf 14,100 MHz. Mit VFO
  14,099 MHz (USB, Ton bei 1 kHz im Durchlass) liest WDSP am HL2 -68 dBm
  roh, am Hermes -73 dBm — die 5 dB sind die Referenz des Simulators
  (Stufe 26 = „0 dB", Longpath sendet bei 0 dB Stufe 31 = +19 dB LNA, so
  wie mi0bot). +10 dB Daempfung: Rohwert -10 dB, Anzeige unveraendert
  (RXOffset = Daempfung + Kalibrierung, wie Thetis).
- **TUNE-Leistung.** Voreinstellung „Use Drive Slider" wie Thetis/mi0bot
  (console.cs:46561): TUNE geht mit dem Drive-Regler raus, der Tune-Regler
  im TX-Applet wirkt erst nach Setup → Transmit → „Use Tune Slider". Dann
  der HL2-Sonderweg aus mi0bot: Ton traegt den Pegel (ToneMag 0,41 bei
  1 W), Drive-Byte 0 → 0,2 W statt 6,7 W am Simulator. Beides gepruft,
  beides wie die Vorlage.
- **MOX mit PC-Mikrofon** (HL2 hat keine Buchse, Quelle ist verriegelt),
  zweiter Empfaenger auf eigenem DDC (7,05 MHz kommt als RX FREQ2 an).

Funde:

- **`RadioStatus` erfuhr nie vom Senden.** Kein Produktionscode rief
  `setActivePttSource()`/`setTransmitting()`; die Diagnoseseite „Radio
  Status" zeigte bei jedem Senden „— W", „RX (idle)", PTT-Quelle „none",
  leere Ereignisliste. Jetzt am `MoxController::moxStateChanged` (Quelle
  aus TUNE/2-Ton am Modell, sonst aus dem Thetis-PTTMode). Regression
  `tst_radio_status_follows_mox`.
- **Die Diagnoseseite selbst:** sass am unteren Rand unter einer leeren
  halben Seite (nach dem Stretch des SetupPage eingehaengt), hatte einen
  zweiten Scrollbereich im Scrollbereich, zeigte „PA Temp 0,0 °C", obwohl
  die Statusleiste 31,5 °C wusste (nur auf Aenderung gehoert, nie den
  Startwert gelesen), Zeitstempel als rohe Epochensekunden und eine
  „Uptime", die mit dem Oeffnen der Seite bei 00:00 begann. Alles behoben.

Nicht bewertet: `rx2Enabled()` bleibt bei zwei Scheiben falsch, weil
`setActiveRxCountLive()` keinen Aufrufer hat (TCI-Init-Burst meldet
`rx2_enable=false`); Thetis' RX2-Begriff deckt sich nicht mit Longpaths
Scheiben — eine Entscheidung, kein Fix.

## Runde 3 (2026-09-22): Filterplatine, und zwei Funde daneben

**Neue Station: N2ADR am Open-Collector-Bus.** Der HL2 hat kein
Alex-Board; sein Vorfilter beim Empfang und sein Oberwellenfilter beim
Senden ist die N2ADR-Platine an denselben sieben OC-Pins. Longpath fuellt
die `OcMatrix` beim Verbinden aus dem N2ADR-Preset (Vorgabe: ein),
`buildCodecContext()` liest daraus je Band und je Senderichtung ein Byte.
Die Werkbank stellt 40 m ein, schaltet auf Senden und zurueck, stellt
20 m ein und vergleicht die Folge mit dem, was der Simulator als
`OpenCollector=` gemeldet hat:

```
OC am Draht: 0x48 0x08 0x48 0x08 0x48 0x08 0x48 0x00 0x48 0x44 0x04 0x44 0x48
erwartet:                                              0x44 0x04 0x44 0x48
```

20 m empfangen 0x48 (Pins 3+6), 20 m senden 0x08 (Pin 3), 40 m 0x44/0x04
— alles richtig. Die 0x00 dazwischen ist kein Fehler, sondern die
dokumentierte Bypass-Entscheidung: zwei Scheiben auf verschiedenen
Baendern (14,2 MHz und 7,05 MHz) koennen nicht dasselbe Vorfilter
brauchen, also nimmt Longpath die Platine beim Empfang aus dem Weg
(`P1RadioConnection.cpp`, `kAlexBypassSentinel`).

**Fund 1 — Longpath stuerzte bei JEDER Verbindung ab, wenn die
NNR-Gewichte fehlen.** `RadioModel::connectToRadio()` legt den
Empfangskanal an und schiebt danach die NNR-Parameter nach
(`setNnrTuning`). Ohne Modelldatei steigt `nnet_build()` vorher aus,
`n->df` bleibt NULL — und `setAlpha_nnet`/`setKnee_nnet` (und ihre beiden
Abfragen) fassten diesen Kopf als einzige Funktionen der Datei ungeprueft
an. `nnr.c` laeuft mit `NNR_ALL_MODELS()` ueber jeden angelegten NNET,
nicht nur ueber die geladenen: ein `SetRXANNRAlpha()` genuegte.
SIGSEGV, mitten im Verbinden. Upstream WDSP 2.10 hat dieselbe Luecke
(gegen einen zweiten, unabhaengigen Klon derselben Ausgabe geprueft);
die Wache ist in `nnet.c` dokumentiert, Pruefstand
`tst_nnr_without_model` (am alten Code: SIGSEGV).

**Fund 2 — NNR lud sein Modell nie.** Derselbe Block setzte den
Modellpfad (`SetNNRModelPathSlot`) **nach** `createRxChannel()`. Die
NNET-Objekte entstehen aber beim Anlegen des Kanals und lesen den Pfad
dabei genau einmal; `SetNNRModelPathSlot` legt hinterher nur eine
Zeichenkette ab. Der erste Kanal jeder Sitzung lief also als Durchreiche
(„nnet: no usable model … passing audio through"), waehrend das
Protokoll gleich darauf „NNR: loading model slot 0 from …" meldete. Der
Block steht jetzt vor `createRxChannel()`; im Protokoll steht seither
`nnet: model loaded — ch 16/32/48/64, …`. Dazu kopiert CMake die beiden
`.bin`-Dateien (2 + 4,5 MB) wie das DeepFilterNet3-Modell ins Bundle,
neben die Binaerdatei und ins Install-Praefix — vorher fand ein
installiertes Paket sie ueberhaupt nicht.

## Runde 4 (2026-09-22): der Mikrofonweg

Neue Station: die fuenf Schalter aus Setup > Audio > TX Input. Der
Simulator meldet `MIC BOOST`, `LINE IN`, `TIP/Ring`, `MicBias` und
`LineGain`; am HL2 fehlen die ersten beiden, weil dort dieselben Bits
Q5/PA/Tune tragen — die Werkbank prueft, was das Board hergibt, und
laeuft darum fuer diese Station am besten gegen `-hermes`.

**Fund:** keines der fuenf Bits kam an. `RadioConnection` kann sie seit
3M-1b (`setMicBoost`, `setLineIn`, `setMicTipRing`, `setMicBias`,
`setMicXlr` — jedes mit Quelle und Polaritaet dokumentiert), die
Setup-Seite schreibt sie ins `TransmitModel`, und dazwischen war nichts:
`micBiasChanged` & Co. hatten als einzige Empfaenger die Oberflaeche
selbst. Der Kommentar in `TransmitModel::setMicBias` sagt es sogar —
„Phase G wires the SetMicBias() bit; model just stores + signals". Phase
G hat den Draht gelegt, angeschlossen hat ihn niemand.

Jetzt haengen alle fuenf an `RadioModel::connectMicInputSignals()`, nach
dem Muster von `connectMicPttDisabledSignal()`: queued binden und einmal
vorladen. Am Simulator:

```
MIC BOOST= 00000001      (beim Verbinden vorgeladen)
LINE IN=   00000001
TIP/Ring=  00000001
MicBias=   00000001
LineGain=  00000011  (17)
```

## Runde 6 (2026-09-23): der Simulator lernt I2C — und das Geraet ging auf Sendung

Der HL2 haengt seine I/O-Platine und die N2ADR-Filterplatine an einen
I2C-Bus, den die Firmware durchreicht. Longpath legt eine Leseanfrage in
den C&C-Rahmen (C0 = Sendebit | Bus<<1 | Anforderung<<7, C1 = 0x07 lesen,
C2 = 0x80|Geraet, C3 = Register); das Geraet antwortet im Empfangsstrom
mit gesetztem Bit 7 in C0. Der Simulator nahm die Anfrage bisher
entgegen und schwieg — dieser ganze Weg war nicht pruefbar.

**Sechster Bench-Patch** (`hpsdrsim.c`, gebaute Fassung in
`~/Longpath/hpsdrsim-bench`): ein winziges Geraetemodell. Adresse 0x41
Register 0 gibt 0xF1 (die Software liest das als „I/O-Platine, Fassung
1"), jedes andere Register ein erkennbares Muster (0xA0|reg …). Die
Antwort belegt den naechsten ep6-Unterrahmen: C0 = 0x80 | Adresse, C1–C4
die Daten; die Abtastwerte desselben Rahmens bleiben unberuehrt, nur die
Telemetrie faellt einmal aus. Mit `WB_NO_I2C=1` bleibt das Modell stumm
(so isoliert man, ob ein Fund an der Antwort haengt — genau so wurde der
Fund unten eingekreist).

**Fund: eine Antwort der Platine schickte das Geraet auf Sendung.**
Sofort beim ersten Lauf mit Antworten stand der Rauschflur 70 dB zu hoch
und der S9-Ton war weg. Mit `WB_NO_I2C=1` war alles normal — also lag es
an der Antwort. Im Protokoll des Simulators:

```
06:08:13.524  WERKBANK I2C READ dev=0x41 reg=0x00 -> C0=0xfd … C4=0xf1
06:08:13.584             PTT= 00000001 (         1)
```

C0 = 0x80 | 0x7D. Bit 7 ist die Antwortmarke, 0x7D die zurueckgegebene
Adresse — und deren Bit 0 las `parseEp6Frame()` als Mikrofon-PTT. Der
Telemetriezweig ueberspringt Antwortrahmen laengst (`if (c0 & 0x80)
continue;`), die PTT-Auswertung stand darueber und las sie mit. Am
echten HL2 mit I/O-Platine ist das dieselbe Lage: das Geraet sendet,
ohne dass jemand etwas drueckt, und bleibt auf Sendung, solange die
Antworten kommen.

Behoben; Pruefstand `tst_p1_i2c_response_is_not_ptt` (ohne Simulator,
vier Faelle inklusive „echtes PTT kommt weiter durch"). Die Werkbank hat
dazu die Station 4i: nach dem Verbinden muss `IoBoardHl2::isDetected()`
gelten und die Hardware-Version 0xF1 sein — das erste Mal, dass dieser
Weg ganz gelaufen ist.

