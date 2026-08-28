# KiwiSDR Receive Client — Design & Status

Status: **shipped**, Stufen 1–6 + 7a. Stufe 7 (Bandrückruf, virtuelle
Antennen, Diversity) ist bewusst nicht gebaut — siehe unten, kein
vergessener Rest.
Companion tests: `kiwi_sdr_protocol_test.cpp`, `kiwi_sdr_redirect_policy_test.cpp`,
`kiwi_public_directory_test.cpp`, `kiwi_sdr_manager_password_test.cpp`,
`kiwi_sdr_manager_csv_test.cpp`, `kiwi_sdr_tx_mute_policy_test.cpp`,
`kiwi_sdr_trace_math_test.cpp`, `kiwi_rebind_tracker_test.cpp`,
`tst_kiwi_is_reachable.cpp`, `tst_kiwi_applet_shows_receivers.cpp`,
`tst_kiwi_display_source.cpp`, `tst_kiwi_tx_mute.cpp`,
`tst_kiwi_sdr_safety_gate.cpp`.

## What this documents

Longpath als **KiwiSDR-Client** — ein empfangsseitiger, fremdbetriebener
Web-SDR (`http://kiwisdr.com`, Protokoll von John Seamons ZL/KF6VO), den
der Betreiber als zusätzliche Empfangsantenne neben oder statt einem
eigenen Funkgerät nutzen kann. Anders als jeder andere `RadioConnection`-
Zweig speist ein KiwiSDR keine DDC/WDSP-Kette — er liefert fertig
demodulierten Ton und fertige Wasserfallzeilen direkt an, genau wie
SunSDR2 QRP via TCI (`2026-08-24-sunsdr-tci-client-design.md`), nur über
ein anderes Protokoll und ohne Steuerkanal zurück.

Bislang gab es kein eigenes Design-Dokument dafür — nur die Kopfkommentare
der einzelnen Dateien und `MainWindow_KiwiSdr.cpp`s eigener, mitwachsender
Kommentarblock. Dieses Dokument fasst das zusammen und hält fest, welche
Entscheidung an welcher Stelle getroffen wurde.

## Herkunft

Ein-Nacht-Port aus AetherSDR (`@31b29583`, 2026-08-23), Stufen 1–4 direkt
im Anschluss, Stufe 6 (Wasserfall) und die Erreichbarkeits-Lücke (6b) am
selben und folgenden Tag, Stufe 7a (Sendesperre) und die
SunSDR-Sicherheitsschranke am 24./25.08. Stufe 5 (Ton in die Mischung)
war bis zum 27.08. offen — siehe unten.

## Architektur: der Weg vom Kiwi-Profil zur Scheibe

Aethers eigene `wireKiwiSdr()` ist rund 400 Zeilen und verbindet Ton,
Wasserfall, virtuelle Antennen, Bandrückruf und Diversity in einem
gemeinsamen, gewachsenen Pfad mit eigener Rauschminderung je Quelle
(`m_kiwiSdrNr2`, `m_kiwiSdrRxBuffer` und ein Dutzend weiterer Felder in
AetherSDRs `AudioEngine`). Longpath geht bewusst einen schmaleren Weg:
eine Kiwi-Scheibe soll sich wie jede andere Scheibe verhalten, ohne
zweites Regelwerk daneben (siehe `AudioEngine.h` Kommentar bei
`feedKiwiSdrAudioData`).

Der Manager (`KiwiSdrManager`) kennt nur **Profile** (ein Kiwi-Endpunkt,
identifiziert über eine `QString id`), nicht Scheiben. Die Brücke in
`MainWindow_KiwiSdr.cpp` — `wireKiwiSdr()` — übersetzt zwischen den
beiden Welten über `assignedSliceForProfile(id)`, für jeden der drei
Verbraucher gleich:

| Verbraucher | Signal | Ziel |
| --- | --- | --- |
| Ton | `decodedAudioReady(id, pcm)` | `AudioEngine::feedKiwiSdrAudioData(sliceId, pcm)` |
| Wasserfall | `waterfallRowReady(id, panId, bins, …)` | `SpectrumWidget::updateKiwiSpectrumDbm(...)` der Scheibe |
| Sendesperre | `TransmitModel::moxChanged`/`tuneChanged` | `AudioEngine::setKiwiSdrAudioSourceEnabled(sliceId, …)` |

`feedKiwiSdrAudioData` tastet den Ton nur um (24 kHz Stereo float, kein
I/Q) und ruft dann `rxBlockReady` — von da an läuft Kiwi-Ton durch
denselben Mixer-Pfad wie jeder Scheibenton: Lautstärke, Stummschaltung,
Schwenk, Anti-VOX, VAX-Abgriff, QSO-Aufnahme, MOX-Sperre, alles ohne
Sonderfall.

## Die eine Sicherheitsschranke: `kiwiControllableSlice`

Übertragen aus der SunSDR-Durchsicht (2026-08-24, siehe dortiges
Dokument): eine Scheibe mit echter DDC-Bindung (`streamIndex() >= 0`,
also ein echtes, womöglich sendefähiges Funkgerät) darf niemals von
einem KiwiSDR-Profil gefüttert oder gesteuert werden — nicht Ton, nicht
Wasserfall, nicht Sendesperre. `MainWindow::kiwiControllableSlice(int)`
ist die einzige Stelle, die das prüft; jeder der drei Verbraucher oben
geht durch sie, statt `sliceById()` von Hand zu wiederholen. Genau das
Auseinanderlaufen (Ton- und Panadapter-Pfad hatten ursprünglich gar
keine Prüfung) war der ursprüngliche SunSDR-Fund und ist hier von
Anfang an vermieden.

## Sendesperre: einfacher als Aethers Latch, mit Begründung

Aethers `KiwiSdrTxMuteLatch` unterscheidet Aussendungen, die der eigene
Rechner ausgelöst hat, von fremden (VOX, CAT, ein anderer Client) — weil
bei FLEX mehrere Clients an einem Gerät hängen können. Bei HPSDR gibt es
diesen Fall nicht: Longpath ist der einzige Client, MOX/TUNE *ist* der
Sendezustand. `KiwiSdrTxMutePolicy` ist mitportiert und geprüft
(`kiwi_sdr_tx_mute_policy_test.cpp`), aber absichtlich nicht verdrahtet
— ein Riegel ohne zweite Eingangsgröße wäre eine Verkomplizierung ohne
Wirkung. `syncKiwiSdrTransmitMute()` (`MainWindow_KiwiSdr.cpp`) fährt
stattdessen direkt auf `setKiwiSdrAudioSourceEnabled`, mit den zwei
mitportierten Profil-Feldern `keepAudioDuringTx` und
`resumeAudioAfterTxDelay`.

## Öffentliches Verzeichnis: `ext_api`-Ehrlichkeit

`KiwiPublicDirectory` liest `kiwisdr.com/public/` und wertet je
Empfänger dessen `ext_api`-Policy aus (0 = API deaktiviert, sonst
API erlaubt, ggf. mit reservierten Web-Kanälen). `KiwiPublicReceiverPicker`
blendet Empfänger mit deaktivierter oder unbekannter API-Policy komplett
aus dem Dialog aus — es wird kein Verbindungsversuch gegen den Willen
des Betreibers unternommen. Der Abruf ist strikt manuell (ein Klick =
ein Abruf), mit ehrlichem User-Agent, kein Hintergrund-Polling.

## Was fehlt — Stufe 7, eine offene Entscheidung, kein Bug

`KiwiRebindTracker` und `KiwiSdrTraceMath` sind vollständig portiert und
für sich geprüft (`kiwi_rebind_tracker_test.cpp`,
`kiwi_sdr_trace_math_test.cpp`), aber an keiner produktiven Stelle
eingebunden. Sie decken Aethers Bandrückruf-Verhalten ab (eine
Kiwi-Zuordnung übersteht einen Bandwechsel und wird beim Zurückwechseln
wiederhergestellt) — bei Longpath bislang: eine Scheibe verliert ihre
Kiwi-Zuordnung ersatzlos, sobald sich Band/Frequenz-Kontext ändert, wie
jede andere unportierte Komfortfunktion.

Das ist absichtlich als eigener Entscheidungspunkt offengehalten, nicht
als Bug behandelt: Bandrückruf, virtuelle Antennen je Scheibe und
Diversity sind laut `CLAUDE.md`s Autonomiegrenzen Feature-Umfang und
Architektur, keine Fehlerkorrektur mit klarer Ursache — sie brauchen
eine bewusste Entscheidung des Betreibers, nicht eine automatische
Ergänzung.

## Ton in die Mischung — nachgezogen 2026-08-27, in zwei Schritten

`feedKiwiSdrAudioData` und `decodedAudioReady` existierten seit dem Port,
waren aber nie verbunden — der Applet zeigte einen grünen, verbundenen
Empfänger, dessen Ton nirgends ankam. Erster Schritt: in `wireKiwiSdr()`
nach demselben Muster wie der Wasserfall-Block direkt darunter
verdrahtet — Profil-ID über `assignedSliceForProfile` auf eine Scheibe
abbilden, durch `kiwiControllableSlice` gattern, erst dann füttern.

Das allein reichte nicht: `feedKiwiSdrAudioData` schreibt nur, solange
`AudioEngine::kiwiSdrAudioEnabled(sliceId)` zustimmt, und
`setKiwiSdrAudioSourceEnabled(sliceId, true)` wurde bis dahin
ausschließlich aus `syncKiwiSdrTransmitMute()` heraus gerufen — also
erst beim ENDE einer eigenen Aussendung. Ohne einen einzigen
Sendezyklus nach dem Verbinden blieb die Quelle dauerhaft
deaktiviert. Zweiter Schritt: `KiwiSdrManager::audioSourceEnabledChanged`
(das der Manager beim Verbinden bereits korrekt auf `true` setzt) mit
derselben Profil→Scheibe→Schranke-Kette an die AudioEngine
angeschlossen, unter Rücksicht auf eine laufende eigene Aussendung
(kein Aufheben der Sendesperre durch dieses Signal, außer
`keepAudioDuringTx` ist gesetzt) — plus `audioSourceRemoved` →
`removeKiwiSdrAudioSource`, das vorher nur beim expliziten Auflösen der
Scheiben-Zuordnung lief, nicht beim Entfernen des Profils selbst.

Alle 13 Kiwi-Testdateien grün nach beiden Änderungen
(`ctest --test-dir build-tests -R kiwi`), Longpath-Zielbuild sauber.

## KIWI-WASSERFÄLLE-Panel (2026-08-27, Longpath-eigen)

Umsetzung von Variante C aus `2026-08-27-kiwisdr-self-report-concept.md`:
ein eigenes, andockbares Panel (`KiwiWaterfallPanel`) mit einem
Mini-Wasserfall (`KiwiWaterfallStripWidget`) je Profil, dessen Vorschau
eingeschaltet ist. Der Schalter dafür sitzt im KiwiSDR-Applet
(`blueToggle`, ein Knopf je Zeile) — bewusst getrennt vom Panel selbst,
das nur Anzeige ist.

Dabei einen echten Fehler gefunden und mitbehoben: `connectProfile()`
verband ein Profil ohne Scheiben-Zuordnung nie wirklich, weil es auf
`profileNeedsInitialTracking` wartete — ein Signal, das im ganzen
Programm nirgends beantwortet wird. Ohne die neue
`waterfallPreviewEnabled`-Abzweigung (die diese Wartestufe für reine
Vorschau-Verbindungen überspringt) hätte der Knopf nie funktioniert.
Das erklärt vermutlich auch, warum die drei am 2026-08-27 zum Test
hinzugefügten öffentlichen Empfänger nie im "Verbunden"-Zustand
blieben.

`shouldMaintainProfileConnection()` berücksichtigt das neue Feld
ebenfalls — eine Vorschau hält die Verbindung offen, genau wie
`autoConnect` oder eine Scheiben-Zuordnung es schon tun, und
`setWaterfallPreviewEnabled(false)` trennt wirklich, sofern nichts
anderes die Verbindung noch braucht (siehe die Höflichkeits-Anforderung
im Konzeptdokument).

## Geprüft und für unbedenklich befunden (2026-08-27)

Zwei von AetherSDRs Commit-Historie nahegelegte Lücken erwiesen sich bei
genauerem Lesen als bereits geschlossen bzw. nicht anwendbar:

- **Verbindung freigeben, wenn keine Scheibe sie mehr braucht** (Aethers
  #3971) — bei Longpath bereits vorhanden: `clearSliceAssignment()`
  ruft `disconnectProfile()`, sobald `shouldMaintainProfileConnection()`
  verneint (Kommentar zitiert #3950).
- **Wasserfall-Historie beim Freigeben löschen** (Aethers #4199/#4202) —
  entfällt: Longpath hat keinen solchen Zwischenspeicher, weil es
  Aethers separates Rauschminderungs-/Puffersystem je Quelle nicht
  mitgebaut hat (s.o., "Architektur").
- **Der von Aether dokumentierte Shutdown-Crash**
  (`KiwiSdrManager::~KiwiSdrManager()` → `disconnectAll()` → Rückruf
  während Teardown) — Longpaths `destroyClient()` trennt die
  Signalverbindung zum Client (`c->disconnect(this)`) synchron, bevor
  die asynchrone Aufräumarbeit (`deleteLater` über
  `QMetaObject::invokeMethod`) überhaupt läuft; ein Kaskadieren in die
  Fensterebene während der Zerstörung ist dadurch unterbunden. Nicht
  live gegen einen Absturz getestet (dazu bräuchte es einen echten
  Lauf, siehe `CLAUDE.local.md`), aber am Code nicht reproduzierbar.
