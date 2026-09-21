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
(Klon liegt unter `~/Longpath/deskhpsdr`). Zwei Zeilen im `main()` von
`hpsdrsim.c` sind fuer die Werkbank noetig:

```c
setvbuf(stdout, NULL, _IOLBF, 0);                 /* Zeilenpuffer, sonst kommt
                                                     das Protokoll erst beim Ende */
addr_udp.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* nur 127.0.0.1 — das Ding
                                                     antwortet sonst im ganzen LAN
                                                     auf jede Discovery */
```

Bauen (macOS, ohne `MacOS.c`):

```bash
cc -O2 -Wno-everything -o hpsdrsim hpsdrsim.c newhpsdrsim.c -lpthread -lm
```

Starten, z. B. als Hermes-Lite 2 (weitere: `-hermes`, `-hermeslite`,
`-angelia`, `-orion2`, `-g2`, dazu `-anan10e` fuer den 10E-Zuschnitt):

```bash
./hpsdrsim -hermeslite2 > sim.log 2>&1 &
```

Port 1024/UDP. `-hermes2` trifft in der Optionsliste zuerst auf `-hermes`
und liefert deshalb ebenfalls den Hermes.

## Werkbank fahren

Beide Pruefstaende **ueberspringen sich selbst** (`QSKIP`), solange
`LONGPATH_HPSDRSIM` nicht gesetzt ist — die CI hat keinen Simulator, und ein
Test, der einen braucht und keinen findet, wuerde dort still scheitern.

```bash
cmake --build build --target tst_hpsdr_sim_workbench tst_hpsdr_sim_gui_workbench

# Modell allein: Discovery → Verbinden → Empfangsstrom → Frequenz →
# Abtastrate → TUNE ein/aus → Trennen; danach liest er sim.log und prueft,
# dass 14,2 MHz, die neue Rate und PTT=1 ankamen und der LETZTE PTT-Stand 0 ist.
LONGPATH_HPSDRSIM=127.0.0.1:1024 LONGPATH_HPSDRSIM_LOG=$PWD/sim.log \
  QT_QPA_PLATFORM=offscreen ./build/tests/tst_hpsdr_sim_workbench

# Mit echtem MainWindow: vier Bildschirmfotos (vor, verbunden, 40 m, getrennt)
# nach LONGPATH_GRAB_DIR. GPU-Flaechen werden per grabFramebuffer() eingelesen,
# deshalb OHNE offscreen — das Fenster erscheint kurz auf dem Schirm.
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
