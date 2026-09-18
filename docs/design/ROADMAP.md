# Roadmap — Longpath

**Stand:** 17. September 2026, Abend. Diese Datei ist die eine Liste,
in der steht, was gebaut ist, was als Nächstes kommt und was dafür vom
Betreiber gebraucht wird. Sie wird bei jedem Schritt nachgezogen; der
Chat ist kein Ersatz dafür.

Die Richtung für alles Sichtbare heißt seit dem 17.09. **„Glas & Tiefe"**
(Stilblatt 3 von vier, vom Betreiber gewählt): Flächen versenkt,
Knöpfe erhaben, Zahlen in Glaschips, Kurven mit Hof, Auswahl als
gedeckter Blauverlauf. Die Grundregeln aus [`HAUSSTIL.md`](HAUSSTIL.md)
bleiben; die Richtung ist ihre Ausformung. Die Farbschicht darunter
(Theme-Datei, `Style::themed()`, Polish-Filter) ist seit August fertig
und trägt das alles — sie steht hier nicht mehr als eigene Baustelle.

---

## Erledigt (17.09.)

| Schritt | Was | Commit |
| --- | --- | --- |
| Durchsicht | Befundliste Bandfilter / Technik / Gestaltung, [`2026-09-17-design-durchsicht.md`](2026-09-17-design-durchsicht.md) | e17e8612 |
| 1 | Bandfilter neu gezeichnet, Achse auf runden Frequenzen, Bedienzeile mit Versalzeilen und Glasfeldern, Panelkopf versal | 3ad720dc |
| 2 | TX-Feld (Rinnen versenkt, Bernstein-Verlauf), Regler, Wertchips, Messingtaste, Kopfleisten-Pillen erhaben mit gedeckter Auswahl | 2d5020a4 |
| 3 | Platten für schwebende Fenster und gedockte Zellen (Verlauf, Rahmen, Luft dazwischen), Fenstertitel versal | ab7104bc |
| 4 | Fußleiste: „ON AIR" passt; zentraler Glas-Tabellenstil (Logbuch im Rotor-Fenster); Bandfilter ohne Gerät `——`; DVK-Schriftwarnung | dee85dc3 |
| 5 | Fußleiste: CH 1 kommt nicht mehr allein zurück (Prüfstand am echten Fenster) | 3290b2ee |
| 6 | Frequenz-Applet/Instrumente: Strich statt Null, Einheit nur wo sie passt, PK/LIM | 56f0a915 |
| 7 | Kopfleiste: Wunsch-Knopf grau, Lautstärke wie jeder Regler, Lautsprecher als Zeichnung | 0f01ff0e |
| 8 | Sechs weitere Tabellen auf den Glas-Tabellenstil, Zebra-Farbe | 89e9d5d8 |
| 9 | Drift-Ratsche wieder grün (61 namenlose Farben, 0 Schriften daneben) | 9bb73c13 |
| 10 | Applets, erste Runde: Auswahlblau gedeckt, Knöpfe nie schmaler als ihr Text, VAX-Pegel in Bernstein, RTTY-Doppelkopf, Bandfilter-Enge | a46ec862 |
| 11 | Sichern, ohne auf das Beenden zu warten: Autosave jede Minute bei Änderung, Tageskopien `Longpath.settings.<JJJJ-MM-TT>` (14 bleiben) | f1f5dd53 |
| 12 | Mitschrift: Whisper-Dienst aus Longpath heraus starten (Setup → Spracherkennung → „Dienst auf diesem Rechner", Automatik beim Einschalten), Applet im Glas-Look; läuft der Dienst schon (Login-Dienst), wird er benutzt statt verdoppelt | 19903b6f |
| 13 | `LONGPATH_CONFIG_DIR`: eigener Konfigurationsordner für eine Prüf-Instanz — die Sandbox mit `HOME=` las auf macOS trotzdem die echten Einstellungen (und meldete sich mit dem Rufzeichen am Cluster an) | b7edab81 |
| 14 | „Nereus" verschwindet, Teil 1 — alles Sichtbare: Log-Kategorien `longpath.*`, Log-Dateien `longpath-*.log`, TCI-Servername, Schlüsselbund mit Umzug, VAX-Geräte „Longpath VAX n"/„Longpath TX" (Plug-in + Erkennung beider Namen), Exporte (ADIF/Cabrillo/KML), Kennungen nach außen, Menüs/Dialoge; Nebenfund: fünf tote `NereusSDR--`-Stylesheets (Fußleisten-Abzeichen ohne Hintergrund seit 20.08.) | 2ea2aa00 |
| 15 | Teil 2 — Bau-Namen (`LONGPATH_*`, `longpath_add_test`), ADIF-Feld `APP_LONGPATH_QRZUP` (altes wird gelesen), CLAUDE.md-Kopf, HOW-TO-PORT, HAUSSTIL, STYLEGUIDE | 2217cca5 |
| 16 | Teil 3 — Kopfzeilen („Modification history (Longpath)", `(Longpath)`, „Longpath-original") und Kommentare in ~1 570 Dateien; Herkunft, datierte Historienzeilen und Übergangs-Erkennungen bleiben | 355f2f17 |
| 17 | Formularstil (C): eine Definition für Auswahlfelder (erhaben), Häkchen/Wahlpunkte (versenkt), Glasfelder, Rinnen waagrecht/senkrecht (EQ), Gruppen, Knöpfe, Bildlaufleisten — auch als App-Basislinie, damit Ungestyltes im Haus-Look erscheint; AGC-T-Regler und DSP-Popup auf die Haus-Rinne | 8e3d0da2 |
| 18 | Setup-Dialog, erster Gang (F): Haus-Reiter (`kTabStyle`), DSP-Seiten, SpectrumPeaks, FilterPresets, Transmit, Display, VAX, Mitschrift, AudioAdvanced, DeviceCard, Container-Editoren auf die Bausteine; Ratsche 1 477 → 1 230 | af9466d2 |
| 19 | Applets, zweite Runde (C): RX-Applet bricht unter 420 px um (Spalten untereinander), Achse ohne Randkollision, CAT-Glaschips, Rille statt heller HLine | 846aa3af |
| 20 | Setup, zweiter Gang (F): Seitenliste mit gedeckter Auswahl, Zeilenschalter mit ON/OFF statt textloser Knöpfe, Label-Spalte 210 px mit Umbruch | 59b1a616 |
| 21 | Panadapter-Chrom (G): dBm-Pfeile und LIVE als erhabene Chips, CH-Tag als Glaschip, kein „◀ 0.0000" ohne Gerät; zentrale Mal-Helfer `Style::paint*Chip` | 88689c3e |

Werkzeuge, die dabei entstanden sind und bleiben: `tst_filter_pane_sheet`
(Bandfilter-Fläche in Betriebsgröße), `tst_tx_entwurf_sheet` mit
`gebaut` / `kopfleiste` / `platte` / `logbuch` / `applets` / `mitschrift`
(echte Widgets als Blatt), `tst_real_status_bar_chain_indicators`
(Fußleiste am echten Fenster). Live-Prüfung ohne Funkgerät: Sandbox-
Instanz mit `LONGPATH_CONFIG_DIR=<Kopie der Einstellungen, Rufzeichen
N0CALL, Cluster-Automatik aus>` und `LONGPATH_AUTOMATION=1` (`dumpTree`,
`grab`). **Nicht** `HOME=` — das greift auf macOS nicht (Schritt 13).

---

## Als Nächstes, in dieser Reihenfolge

### B · „Nereus" verschwindet  *(Betreiber, 17.09.: „wir haben kein nereus … weg damit")*

Erledigt in drei Commits (Tabelle 14–16). Was noch aussteht:

7. **Ordnername** `~/Longpath/NereusSDR` → `~/Longpath/Longpath`. Nicht
   nachts gemacht: Bau- und Startbefehl, der Worktree
   (`.git/worktrees/…` mit absoluten Pfaden), Launch-Agents und die
   zweite Session hängen daran. Wenn du es willst, in einem Zug:

   ```bash
   cd ~/Longpath && mv NereusSDR Longpath && cd Longpath && git worktree repair && git worktree list
   ```

   Danach `rm -rf build` (der Cache trägt den alten absoluten Pfad;
   der nächste Bau ist ein voller, ~10 min) und der Befehl lautet
   `cd ~/Longpath/Longpath && ./build.sh && ./run.sh`. Vorher Longpath
   beenden. Launch-Agents hängen nicht am Pfad (geprüft 18.09.).
8. **HAL-Plug-in (VAX)**: auf diesem Rechner ist keines installiert, es
   gibt also nichts zu ersetzen. Wenn VAX-Geräte gewünscht sind:
   ```bash
   cd ~/Longpath/NereusSDR && cmake -S hal-plugin -B build-hal -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build-hal && sudo cmake --install build-hal && sudo killall coreaudiod
   ```

   (braucht das Passwort) — die Geräte heißen dann „Longpath VAX 1–4"
   und „Longpath TX". Prüfung vorab ohne Passwort: `cmake --build build-hal`
   allein baut `build-hal/LongpathVAX.driver`.

Was bewusst bleibt, steht in `CLAUDE.md` oben (Herkunft/Copyright,
datierte Historienzeilen, Übergangs-Erkennungen, datierte Dokumente
unter `docs/architecture/`).

### C · Übrige Applets, zweite Runde  *(erledigt, 18.09.)*
Formularstil (17), Umbruch/Achse/CAT-Chips/Rille (19). Was bleibt, ist
Feinschliff am Gerät.

### D · Verbindungsdialog
Der erste Bildschirm, den jeder sieht: Versalzeilen statt
Qt-Gruppenrahmen, Glas-Tabelle, gedeckte Auswahl, eine Sprache in der
Fehlerzeile. **Wartet darauf, dass die offenen Änderungen in
`ConnectionPanel.cpp/.h` festgeschrieben sind** (sonst kollidieren wir).

### E · Sprache
Eine Entscheidung des Betreibers: Englisch durchgehend (Empfehlung —
MOX, VOX, BW, S-Meter sind ohnehin englisch) oder Deutsch durchgehend.
Danach eine Durchsicht aller sichtbaren Texte („Bandwidth Filter" neben
„Frequenz", „Leistung" neben „Tune", „Mitschrift", „Leeren").

### F · Setup-Dialog  *(zwei Gänge erledigt, 18.09.)*
Tabelle 18 und 20; Blätter `tst_tx_entwurf_sheet setup|setupdialog`.
Offen: DeviceCard-Pillen, AudioVax-Badge, HardwarePage-Reiter (nutzt
noch keinen `kTabStyle`), Seitentitel als Versalzeile — Kleinigkeiten,
die man am Gerät entscheidet.

### G · Panadapter-Kopf, Overlay, Wasserfall-Palette  *(erster Gang erledigt, 18.09.)*
Tabelle 21. Eine Hausstil-Palette gibt es schon: „Gedämpft" (seit
15.08., folgt dem Theme) — als Vorgabe wählbar unter Setup → Display →
Color Scheme. Offen, weil nur mit Signal beurteilbar: der orange
Bandbalken („ALL"), das Rechtsklick-Menü, die Overlay-Knöpfe links
unten im Wasserfall.

### H · Live am Gerät, dann Release
Bandfilter mit Kurve, TX-Feld beim Senden, Fußleiste mit zwei Ketten —
das Foto, das kein Prüfstand ersetzt. Danach die 0.6.3 (Website
nachziehen, siehe Notiz vom 05.09.).

---

## Was vom Betreiber gebraucht wird

- **Sprache** (E): Englisch oder Deutsch.
- **ConnectionPanel committen** (D).
- **Ordnername** (B7): willst du `~/Longpath/Longpath`? Befehl steht
  oben; ich mache es nicht ungefragt, weil dein Terminal und die zweite
  Session daran hängen.
- **HAL-Plug-in** (B8): nur, wenn du VAX-Geräte willst — Befehl oben,
  braucht das Passwort.
- **Ein Foto mit Funkgerät** (H), sobald das ANVELINA wieder da ist.
