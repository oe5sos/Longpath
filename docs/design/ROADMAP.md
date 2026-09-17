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

Sichtbar zuerst, dann der Rest:

1. **Audiogeräte** „NereusSDR VAX 1–4" / „NereusSDR TX" → „Longpath VAX
   1–4" / „Longpath TX". Die Namen vergibt das HAL-Plug-in
   (`hal-plugin/LongpathVAX.cpp`, `Info.plist`); die App erkennt sie
   per Muster (`VirtualCableDetector`, PipeWire/Linux-Bus, VAX-Seiten).
   Beides ändern, das alte Muster als Übergang weiter erkennen. Das
   Plug-in muss danach neu installiert werden (`/Library/Audio/Plug-Ins/HAL`,
   braucht das Betreiber-Passwort), und WSJT-X/fldigi brauchen einmal
   das neue Gerät in ihren Audioeinstellungen.
2. **TCI-Kennung** „NereusSDR-TCI" → „Longpath-TCI" (`TciServer.cpp`).
3. **Schlüsselbund** „NereusSDR: …" → „Longpath: …" mit Lesen des alten
   Eintrags als Rückweg (`CredentialStore.cpp`), sonst sind gespeicherte
   Zugangsdaten weg.
4. **Log-Kategorien** `nereus.*` → `longpath.*` (`LogCategories.cpp`,
   30 Stellen) — das sind die Präfixe im Terminal-Log.
5. **Texte in Dialogen** (VAX-Ersteinrichtung, Audio-Setup, Über,
   Support-Bundle, Diagnose).
6. **Dokumente und Kommentare**: `HAUSSTIL.md` („NereusSDR — Hausstil"),
   `CLAUDE.md`, die Kopfzeilen „Modification history (NereusSDR)" in
   ~200 Dateien. Nicht sichtbar im Programm, aber sichtbar beim Lesen.
7. **Ordnername** `~/Longpath/NereusSDR` → zuletzt, weil Bau- und
   Startbefehl daran hängen; mit neuem Befehl im selben Schritt.

### C · Übrige Applets, zweite Runde
Qt-Standard-Comboboxen (USB, Preset, Profile, Rate, Buffer, Baud) und
-Checkboxen auf Glas, vertikale EQ-Regler, CAT-Kapseln, Slider-Rinnen in
Tuner/Diversity. Braucht einen App-weiten Formularstil (QComboBox,
QCheckBox, QLineEdit) — derselbe Hebel wie für den Setup-Dialog.

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

### F · Setup-Dialog
Seite für Seite auf Glas, mit dem Formularstil aus C.

### G · Panadapter-Kopf, Overlay, Wasserfall-Palette
Die Pillen CH 0/TX, die dBm-Pfeile, das Rechtsklick-Menü; eine
Hausstil-Palette für den Wasserfall als wählbare Vorgabe (kein Zwang).

### H · Live am Gerät, dann Release
Bandfilter mit Kurve, TX-Feld beim Senden, Fußleiste mit zwei Ketten —
das Foto, das kein Prüfstand ersetzt. Danach die 0.6.3 (Website
nachziehen, siehe Notiz vom 05.09.).

---

## Was vom Betreiber gebraucht wird

- **Sprache** (E): Englisch oder Deutsch.
- **ConnectionPanel committen** (D).
- **Passwort für das HAL-Plug-in** (B1), wenn es so weit ist — den
  Befehl gebe ich dann.
- **Ein Foto mit Funkgerät** (H), sobald das ANVELINA wieder da ist.
