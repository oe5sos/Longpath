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
