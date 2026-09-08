# Midi2Cat — Scope & Design Notes (Recherche, kein Bauauftrag)

**Status:** reine Recherche/Scoping, 2026-09-06. Kein Code geschrieben, keine
Phase begonnen. Dieses Dokument klärt, WAS Thetis' Midi2Cat tut und WIE groß
der Port wäre — die Entscheidung, ob und wann gebaut wird, ist offen (siehe
"Offene Entscheidung" ganz unten).

**Companion:** keiner bisher — dies ist das erste Dokument zum Thema.

## Woher das kommt

Midi2Cat stand als "reale, nennenswerte Lücke" auf der Backlog-Liste aus
einer früheren AetherSDR/Zeus-Recherche, mit einer groben Hausnummer
"~6700 Zeilen" aus einem oberflächlichen Blick auf den `Midi2Cat/`-Ordner
allein. Diese Recherche liest den vollständigen Thetis-Quellbaum
(`../Thetis/Project Files/Source/Midi2Cat/` + `Console/Midi2CatCommands.cs`)
und korrigiert die Größenschätzung sowie klärt Architektur, Befehlsumfang,
TX-Sicherheitsrelevanz und Persistenzformat — bevor irgendetwas gebaut wird.

## Was Midi2Cat ist

Ein MIDI-Controller (typisches Zielgerät: Behringer BCF2000/BCR2000,
CMD PL-1, CMD Micro — siehe die beiden PDF-Anleitungen im Thetis-Repo)
steuert Thetis' eigene CAT-Befehle: VFO-Frequenz, Modus, Filter, Pegel,
AGC, Band usw. per konfigurierbarer MIDI-CC/Note-Zuordnung. Umgekehrt
sendet Thetis bei bestimmten Zustandsänderungen (Drive-Pegel, AGC-Pegel,
CW-Speed, RIT/XIT, VFO-A/B-Lautstärke) MIDI-Rückmeldungen zurück, damit
motorisierte Fader/LED-Ringe am Controller den Live-Zustand des Funkgeräts
zeigen — echte Zweirichtungs-Kopplung, nicht nur MIDI-Eingang.

## Größenkorrektur: ~13.300 statt ~6.700 Zeilen

Die frühere Schätzung zählte nur den `Midi2Cat/`-Klassenbibliotheksordner
und übersah komplett `Console/Midi2CatCommands.cs` — fast genauso groß
noch einmal:

| Teil | Zeilen |
| --- | --- |
| `Midi2Cat/` (24 Dateien, inkl. Designer/Resx-Code) | 6.641 |
| `Console/Midi2CatCommands.cs` (Befehlsimplementierung) | 6.680 |
| **Gesamt** | **13.321** |

Davon sind rund 1.850 Zeilen reiner WinForms-Designer-Code (`.Designer.cs`),
der nicht zeilenweise übersetzt wird, sondern als Qt6-Widget-Layout neu
entsteht. Der tatsächlich zu lesende/portierende Logikanteil liegt bei
grob **11.470 Zeilen** — deutlich mehr als die ursprüngliche Hausnummer,
aber nicht in der Größenordnung, die eine Ablehnung nahelegen würde (zum
Vergleich: die Dev-Automatisierungs-Bruecke, die AetherSDR-seitig auch als
"mehrere Sitzungen, nicht eine" eingestuft wurde, hat dort ~12.000 Zeilen).

## Architektur (Thetis-Seite)

Midi2Cat ist bei Thetis bewusst **radiofremd/generisch** gebaut und nur
über eine einzige Datei mit der Konsole verbunden:

- **`Midi2Cat.csproj`** ist eine eigene .NET-4.8-Klassenbibliothek ohne
  Projektverweis auf Console/Thetis (Referenzen nur: System, WinForms,
  System.Xml.Linq, System.Data, Microsoft.CSharp). Eine `using Thetis;`-
  Zeile in `CatCmdDb.cs` ist totes Altgut, referenziert nie einen
  Thetis-Typ.
- **`Console/Midi2CatCommands.cs`** ist die einzige Brücke. Konstruktor:
  `midiManager = new MidiMessageManager(this, c.AppDataPath + "midi2cat.xml");`
  — `this` wird nur als `Object` übergeben. `MidiMessageManager` kennt
  `Midi2CatCommands` zur Übersetzungszeit nicht; die Bindung läuft über
  Reflection: `Type.GetMethod(mapping.CatCmdId.ToString())` +
  `Delegate.CreateDelegate(...)` — der Name des `CatCmd`-Enum-Werts MUSS
  exakt einer Methode auf `Midi2CatCommands` entsprechen.
- **Live-Dispatch:** rohes MIDI-Byte-Tripel → `MidiDevice` (Win32-
  `winmm.dll`-Callback-Thread) → `MidiMessageManager.OnMidiInput` schlägt
  `(DeviceIndex, ControlId)` in einem `Dictionary<int, ControllerBinding>`
  nach → ruft den gebundenen Delegate auf der `Midi2CatCommands`-Instanz
  auf → diese Methode ruft `commands.ZZxx(...)` — **dieselben** `ZZ`-
  CAT-Befehlszeichenketten, die Thetis auch von einer echten seriellen/
  TCP-CAT-Verbindung parst. **157 `ZZxx`-Aufrufstellen** über ~130
  eindeutige CAT-Codes laufen so. Eine Handvoll Methoden umgeht CAT und
  fasst `Console`-Felder direkt an (Mic-Gain-Grenzwerte, CPDR-Grenzwerte,
  MIDI-Rad-Empfindlichkeit, drei `console.CATVFOAtoB()/CATVFOBtoA()/
  CATVFOABSwap()`-Helfer, damit z.B. der Modus beim VFO-Kopieren
  mitgenommen wird).
- **Rückrichtung:** `console.cs` ruft an ~9 Stellen
  `Midi2Cat.SendUpdateToMidi(CatCmd.X, pct)` auf, sobald sich Drive-Pegel,
  AGC-Pegel, CW-Speed, RIT/XIT oder VFO-A/B-Lautstärke in der UI ändern.
- **Thread-Marshaling:** der MIDI-Eingangs-Callback läuft auf dem rohen
  `winmm`-Callback-Thread, nicht im UI-Thread. `CATCommands.cs` prüft in
  den VFO-Frequenz-Handlern explizit `console.InvokeRequired` mit dem
  Kommentar *"needed as Midi is from another thread"*; ein privates
  `isMidi`/`isMidi2`-Flag unterdrückt Panadapter-Neuzentrierung speziell
  bei MIDI-ausgelösten Frequenzänderungen (Rückkopplungsschutz gegen
  UI-Sprünge während MIDI-Abstimmung).
- **UI-Einstieg/Exklusivität:** der Zuordnungsdialog öffnet aus einem
  "Configure"-Knopf im Setup-Dialog. Beim Öffnen wird der laufende
  `MidiMessageManager` geschlossen (`console.Midi2Cat.CloseMidi2Cat()`)
  und beim Schließen des Dialogs wieder geöffnet (`OpenMidi2Cat()`) — der
  laufende Dispatcher und der Einrichtungsdialog können das physische
  Gerät nie gleichzeitig offen halten.

## Zwei-Quellen-Regel für diesen Port

| Frage | Quelle |
| --- | --- |
| Welche Befehle gibt es, was tun sie, welche sind TX-relevant? | Thetis (`CatCmdDb.cs` + `Midi2CatCommands.cs`) |
| Wie sieht das Zuordnungsformat/die Persistenz konzeptionell aus? | Thetis (`Database.cs`, `.m2c`-Format) |
| Wie wird das in Qt6 strukturiert (Klassen, Signale, Learn-API)? | AetherSDR (`MidiControlManager` + Geschwister, siehe unten) |
| Welche MIDI-I/O-Bibliothek? | AetherSDR (RtMidi, bereits vendort) — Thetis liefert hier nichts Portables |

## Befehlsumfang: 236 nutzbare Befehle (+ 2 tote)

Quelle: `CatCmd`-Enum in `Midi2Cat.Data/CatCmdDb.cs`, 237 Einträge
(236 nutzbar + `None`). Gegengeprüft gegen `Midi2CatCommands.cs`: 238
Methoden mit `CatCmd`-passendem Namen (236 verdrahtet + 2 unerreichbare
Leichen).

Kategorien (Auszug, volle Liste im Recherche-Rohmaterial dieser Sitzung
verfügbar, nicht hier dupliziert um das Dokument lesbar zu halten):
VFO/Tuning (~20), RIT/XIT/Split, Modus-Wahl (32, inkl. RX2-Spiegel),
Filter/Bandbreite, Band-Wahl (26, inkl. RX2-Spiegel), Pegel/AGC (~24),
Rauschminderung/-blanking/Notch/Diversity (~24), CW, Multi-RX/RX2,
Display/Panadapter/Wasserfall, EQ, VAC, Tuner/PA/Leistung/System/Makros.

**Zwei tote Methoden — nicht als lebendes Feature portieren:**
`SpurReductionOnOff` (ruft `ZZSR`) und `PreampFlex5000` existieren als
Methoden, haben aber **keinen passenden `CatCmd`-Eintrag** — die
Reflection-Bindung kann sie nie erreichen, der Einrichtungsdialog kann sie
nie anbieten. Vermutlich durch `AutoNotchOnOff` bzw. die generischen
`PreAmpSettingsKnob`/`Rx2PreAmpOnOff` abgelöst.

## TX-relevante Befehle — eigener Abschnitt, weil sicherheitskritisch

Longpaths TX-Sicherheitshaltung ist nicht verhandelbar
(`CLAUDE.local.md` "Die harte Grenze, die kein Design überschreibt";
`BandPlanGuard`). Jede zukünftige `invoke`-artige Ausführungsschicht für
Midi2Cat-Befehle braucht dieselbe Art getrennter, expliziter Freigabe wie
die Dev-Automatisierungs-Bruecke (`AETHER_AUTOMATION_ALLOW_TX`-Muster,
siehe `2026-09-06-dev-automation-bridge-design.md`) — Lese-/Zuordnungs-
Opt-in darf nie TX-Opt-in implizieren.

**Tastet den Sender direkt:**
- `MOXOnOff` — ruft `ZZTX`, der manuelle TX/MOX-Schalter. Das IST PTT.
- `TunOnOff` — ruft `ZZTU`, startet TUNE, sendet einen Träger.
- `TwoToneOnOff` — sendet ein Zweiton-HF-Testsignal.
- `CWXMacro1`–`CWXMacro9`, `CWXStop` — CW-Keyer-Textmakros; Abspielen
  sendet CW über die Luft.
- `QuickPlayOnOff` — spielt eine aufgenommene Wave-Datei (CQ-Makro) ab
  und tastet dabei TX.

**Bewaffnet oder verändert TX-Verhalten, tastet selbst nicht:**
`VOXOnOff`, `SplitOnOff`/`QuickSplitOnOff`/
`QuickSplitOnOffandSplitOnOff`/`ToggleTX`, `CWBreakIn`/`CWQSK`,
`TunerOnOff`/`TunerBypassOnOff`, `ExternalPAOnOff`.

**Setzt laufende TX-Signalkettenparameter:** `DriveLevel`(+`_inc`),
`TUNPowerLevel`, `MicGain`, `VOXGain`, `TXAFMonitor`, `TXEQOnOff`,
`TXFilterHigh`/`TXFilterLow`, `CompanderOnOff`.

**Nicht TX, aber operationell gefährlich bei Fehlzuordnung:**
`CloseConsole` ruft `ZZBY` und beendet die gesamte Anwendung fernaus­
gelöst — eine versehentliche MIDI-Zuordnung hierauf könnte die Konsole
mitten in einer Sendung beenden.

## MIDI-I/O: Windows-only bei Thetis, RtMidi bei AetherSDR bereits da

`Midi2Cat.IO/WinMM.cs` ist eine rohe `winmm.dll`-P/Invoke-Schicht ohne
jede Abstraktion (kein NAudio, nichts Portables). Gerätespezifische
Eigenheiten sind hart codiert (`getUniqueDevice()`/`filterAndMap()`/
`FixBehringerCtlID()`: Behringer CMD PL-1, CMD Micro, Numark DJ2GO2
Touch, DJControl Starlight — ID-Kollisionsbehandlung, 14-Bit-LSB/MSB-
CC-Filterung, Regler-vs-Knopf-ID-Kollisionen).

**Folge:** Thetis liefert hier keine wiederverwendbare Codezeile, nur
Verhalten zum Nachbauen (Konventionen für Relativ-Encoder, die
Geräte-Eigenheiten-Tabelle, die Feedback-Nachrichten-Vorlagengrammatik).
Für macOS gilt: `~/Longpath/AetherSDR` hat bereits **RtMidi vendort**
(`third_party/rtmidi/`, ~5.275 Zeilen) UND eine vollständige, ausgereifte
Qt6-Struktur obendrauf:

- `MidiControlManager` (`src/core/MidiControlManager.h/.cpp`,
  164+617 Zeilen) — QObject, RtMidi-Wrapper, `registerParam`/
  `addBinding`/`removeBinding`/`clearBindings`/`rebuildIndex`, echte
  Lernen-API (`startLearn`/`cancelLearn`/`learnCompleted`/
  `learnCancelled`), Signale `portOpened`/`portClosed`/`portError`/
  `midiActivity`/`paramValueChanged`/`paramActionTrace`/`relativeAction`.
- `MidiRelativeCcDecoder` — sauberer Mehrschema-Relativ-Encoder-Decoder,
  prinzipieller als Thetis' Ad-hoc-`msg==127`/`msg==1`-Konvention direkt
  in jeder Befehlsmethode.
- `MidiSettings` (111+893 Zeilen) — Persistenz sauber vom Live-Manager
  getrennt (`load`/`save`/`saveBindings`/`setLastDevice`/
  `setAutoConnect`/`deleteProfile`) — klarer geschnitten als Thetis'
  `Midi2CatDatabase`, die Live-Gerätezustand und benannte Profile in
  einer Klasse vermischt.
- `MidiMappingDialog` (`src/gui/MidiMappingDialog.h`) — der Qt6-
  Zuordnungsdialog, strukturell Thetis' `Midi2CatSetupForm`/
  `MidiDeviceSetup` entsprechend.
- `UlanziDialMapperDialog` — eigener Dialog für ein einzelnes
  USB-Dial-Produkt, zeigt dass AetherSDR geräte-spezifische Dialoge
  bereits kennt (Parallele zu Thetis' Behringer-PL-1/Micro-Sonderfällen).
- Verdrahtung über ein Metadaten-Register-Muster in
  `MainWindow_Controllers.cpp`, bewusst ohne Hauptthread-Objekte in
  Lambdas an den MIDI-Manager zu binden (thread-sicherheitsbewusst,
  passend zu Thetis' eigenem Marshaling-Problem oben) — getestet
  (`tests/midi_settings_test.cpp`, `tests/midi_relative_cc_decoder_test.cpp`).
- Zwei verwandte, aber getrennte Subsysteme (`FlexControlManager` für
  serielle Hardware, `HidEncoderManager` für USB-HID-Encoder) zeigen:
  AetherSDR behandelt jeden physischen Steuerpfad als eigene Manager-
  Klasse statt als einen Monolithen — ein Muster, das Longpaths eigenes
  `MidiControlManager`-Äquivalent übernehmen sollte.

**Für den Port:** RtMidi entweder aus AetherSDR übernehmen (gleiche
Lizenz-Lage prüfen, aber es ist bereits als Fremdcode vendort und läuft)
oder frisch vendorn — so oder so, Thetis liefert dafür nichts. Struktur
(`MidiControlManager`/`MidiSettings`/`MidiRelativeCcDecoder`/
`MidiMappingDialog`) ist eine legitime AetherSDR-Zitatgrundlage nach der
Zwei-Quellen-Regel; jedes Verhalten (welche Befehle, was sie tun,
TX-Kennzeichnung, Persistenzform) bleibt Thetis-Domäne.

## Persistenzformat (Thetis) → Vorschlag für AppSettings

Thetis speichert pro Profil-Ordner eine `midi2cat.xml`
(`Midi2Cat.Data/Database.cs`, ein `System.Data.DataSet` via
`WriteXml(..., XmlWriteMode.WriteSchema)`): eine Tabelle pro
physischem Gerätenamen (Spalten `MidiControlId`, `MidiControlName`,
`MidiControlType`, `MinValue`, `MaxValue`, `CatCmdId`,
`MidiOutCmdDown`/`Up`/`SetValue`), plus zusätzliche Tabellen für
benannte, gespeicherte Zuordnungsprofile, plus eine Settings-Tabelle für
z.B. welches benannte Profil gerade aktiv ist. Export/Import läuft über
dieselbe DataSet-XML-Mechanik mit `.m2c`-Dateiendung. Ein paar
Midi2Cat-nahe Einstellungen (`MidiMessagesPerTuneStep`(Min/Max),
`SwapVFOWheelsProperty` — sogar als eigener CAT-Befehl `ZZZW` exponiert)
liegen direkt auf `Console`, nicht in `midi2cat.xml`.

**Für Longpaths `AppSettings`** (PascalCase-XML, siehe CLAUDE.md
"Settings Persistence"): ein Abschnitt mit einer Liste
Geräte→Zuordnungen (Control-ID → Befehl-ID → Typ/Min/Max/Feedback-
Zeichenketten), plus 2-3 einzelne Skalarwerte (Tune-Schritt-pro-
Nachricht Min/Max/aktuell, VFO-Rad-Tausch-Flag) daneben, getrennt von
der eigentlichen Zuordnungsliste. Benannte Profile + `.m2c`-Import/
Export sind ein späterer Ausbau, kein Tag-1-Erfordernis.

## Einrichtungsdialog: sechs Formulare, deutliche Ausmaße

- **`Midi2CatSetupForm`** — dünne Außenhülle: 5 Sekunden Fake-
  "Initialisierung" (CAT/MIDI-Synchronisationssicherheit), dann ein
  Reiter pro erkanntem MIDI-Eingangsgerät.
- **`MidiDeviceSetup`** — der eigentliche Arbeitsplatz, drei Unterreiter:
  - **Zugeordnete Regler**: bearbeitbares Datengitter + Overlay-Panel
    mit bis zu 8 Feldern zum Hinzufügen/Bearbeiten (Name, Typ [Unbekannt/
    Knopf/Regler/Rad], Ziel-CAT-Befehl, Min/Max-Anzeige, 3 aufklappbare
    "Erweitert"-Hex-Vorlagenfelder für Rückmeldungen).
  - **Befehle**: umgekehrter "Lernen"-Ablauf — erst Zielbefehl wählen,
    dann Regler bewegen zum Binden.
  - **Debug**: Live-Rohverkehr-Gitter (max. 100 Zeilen) + Verbindungs-
    status + Fehlerprotokoll — ein echtes MIDI-Traffic-Diagnosewerkzeug.
  - Menü: Speichern unter/Laden/Export/Import/Organisieren — volle
    Profilverwaltung.
- Vier kleine Dialoge (Speichern-unter, Laden, Auswählen, Organisieren).

Das "Lernen" ist real und zweigleisig (Regler zuerst ODER Befehl
zuerst). Dieser Dialogumfang ist einer der größeren Qt6-Widget-Posten
im gesamten Port — vergleichbar mit einem Einstellungsgitter samt
Inline-Modal, Live-Paketmonitor und vier Dateiverwaltungsdialogen.

## Offene Entscheidung — bevor irgendeine Phase beginnt

Dieses Dokument klärt WAS und WIE GROSS, aber nicht OB und WANN. Zwei
Dinge fehlen, die nur der Betreiber klären kann, nicht ich:

1. **Besitzt Martin überhaupt einen MIDI-Controller?** Ohne echtes Gerät
   lässt sich weder die Encoder-Konvention noch die Geräte-Eigenheiten-
   Tabelle am Funkgerät verifizieren — reine Simulationstests wären
   möglich, aber die eigentliche Werkbankprobe bräuchte Hardware.
2. **Will er das Feature überhaupt, und mit welcher Priorität** relativ
   zum übrigen Rückstand (RTL-SDR-Scope-Frage, Green-Heron-Rotor,
   weitere Phase-1-Ausbaustufen der Automatisierungs-Bruecke)? Dies ist
   ein komplett neues, nutzersichtbares Feature (~11.5k Zeilen Logik zu
   lesen/übersetzen plus ein großer Qt6-Dialog) — laut CLAUDE.md
   "Autonomous Agent Boundaries" ausdrücklich kein Bereich, den ich ohne
   Rückfrage begonnen anlegen sollte ("Feature scope — adding features
   beyond what the issue describes").

**Empfehlung, falls Martin zustimmt:** in Phasen wie bei der
Dev-Automatisierungs-Bruecke, mit der TX-Sicherheitsschranke von Anfang
an mitgedacht statt nachgerüstet:

- Phase 1: `MidiControlManager`-Äquivalent (RtMidi-basiert, aus
  AetherSDR strukturell übernommen) + reine Nicht-TX-Befehle (VFO/Modus/
  Filter/Pegel/Band, kein `ZZTX`/`ZZTU`/CWX/Two-Tone/QuickPlay) +
  minimaler Zuordnungsdialog ohne benannte Profile.
- Phase 2: TX-relevante Befehle, explizit hinter einer eigenen,
  getrennten Freigabe (Analogie zu `AETHER_AUTOMATION_ALLOW_TX`) —
  nicht einfach mitgeliefert, weil Phase 1 schon funktioniert.
- Phase 3: volle Dialogparität (benannte Profile, `.m2c`-Import/Export,
  Debug-Reiter, Rückmeldungsnachrichten für motorisierte Fader/LEDs).
- Phase 4: geräte-spezifische Eigenheiten (Behringer PL-1/Micro usw.),
  sobald ein reales Gerät am Werktisch steht.
