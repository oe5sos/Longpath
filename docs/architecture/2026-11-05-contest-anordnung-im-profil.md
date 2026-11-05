# Die Contest-Anordnung gehört ins Layoutprofil (2026-11-05)

**Betreiber, nach einem Neustart auf main@0aa25bd80:** „so war mein
letzter zustand nicht!!!!" Vorher: Karte, Log und Chat als Fenster
neben dem Panadapter, kein Logbuch-Fenster. Nachher: das Logbuch-Fenster
offen, Rotoren darin, Chat und Log an anderen Stellen.

## Befund

Seine Anordnung stand nirgends.

- Das Contestprogramm sichert die Lage eines abgelösten Panels nur bei
  **eigenem Zug** (`geometryEdited`); Longpaths Mitziehen und das
  Raster gingen daran vorbei. Sein gespeicherter Stand stammte vom
  Vortag.
- Longpaths Layoutprofil kannte die Contest-Panels nicht. Sein
  Desktop-Export `CONTEST.json` (06:23) enthält genau das: den
  Panadapter unten links, alles andere ausgeblendet — und keine Zeile
  zu den Contest-Panels.
- „Open contest logbook at launch" öffnete das Logbuch-Fenster bei
  **jedem** Start, auch wenn er es zuletzt zu hatte.
- Das ✕ am eingebetteten Logbuch-Fenster schloss es mit
  `WA_DeleteOnClose`: Controller und Datenbank gingen hinterher, die
  abgelösten Panels (seit #74 Geschwister am Hauptfenster, nicht Kinder
  des Logbuchs) blieben mit totem Controller stehen. Danach sicherte
  niemand mehr etwas.

## Behebung

- **Die Anordnung ist Teil des Profils.** Die Brücke liefert
  `contestPanels` (id → x, y, w, h der Fenster) und `contestLogbook`
  (Fenster sichtbar); die Erfassung schreibt sie in jedes Profil, das
  Anwenden stellt sie her: genau diese Panels als Fenster an ihrer
  Stelle, alle anderen zurück, Logbuch nur sichtbar, wenn es sichtbar
  war. Ein Profil ohne Contest räumt auf, lässt das Contestprogramm
  aber laufen (CAT, Cluster). Damit gehen die Panels auch mit einem
  Export/Import des Profils.
- Ohne geladenes Logbuch bleibt das zuletzt Gelesene im Profil stehen:
  ein Start ohne Contest verliert die Anordnung nicht.
- Jede Änderung (Fenster/zurück, Zug, ✕, Logbuch auf/zu) meldet
  `anordnungGeaendert()`; der Wirt erfasst gedämpft (500 ms).
- „Open contest logbook at launch" greift nur, wenn das Profil nichts
  zum Contest sagt.
- Das ✕ am eingebetteten Logbuch versteckt nur noch; Tools › Contest
  Logbook holt dieselbe Ansicht wieder. Abgebaut wird über
  `schliesse()` beim Beenden.

## Prüfstände

- `tst_contestfenster::dieAnordnungKommtAusDemProfilZurueck` (Werte →
  abbauen → herstellen → dieselben Rahmen, Logbuch zu; aufräumen;
  sichtbar zurück).
- `tst_contestfenster::dasKreuzAmLogbuchVerstecktNur` — rot gegen die
  alte Fassung (`close()` ging durch).
- `tst_contest_panels_im_containers_menu::dasProfilBringtDiePanelsZurueck`
  — rot gegen die alte Fassung (chat kein Fenster).

Nicht gesichert: die Anordnung von 06:18 selbst — sie war nie
gespeichert und muss einmal neu gelegt werden; ab dann bleibt sie.

## Nachtrag: die Contest-Panels im Combined-Feld (2026-11-05, nachmittags)

**Betreiber, mit Bild:** „im combined sollten auch alle contest
optionen möglich sein" — die Liste hinter dem „+" des Combined-Felds
kannte nur Longpaths Applets und Rotor/Log.

Jetzt stehen dort alle Panels des Logbuchs („Contest · Karte /
Verbindungen", …). Der Weg ist derselbe wie bei Rotor/Log: das Feld
nimmt das Widget selbst auf (`KombiApplet::nimmAuf`), ohne Hülle.

- **Contestprogramm (#76):** `PanelContainerWidget::setzeFremdesGehaeuse`
  — im Feld ohne eigene Knöpfe (Schloss, ↙, ✕) und Anfasser, und
  `setFloating(false)` tut nichts: ein Andocken aus dem Logbuch heraus
  risse das Widget aus dem Feld. Für den PanelLayoutManager bleibt es
  „abgelöst".
- **Brücke:** `panelFuerFremdesGehaeuse()` macht das Panel bereit
  (geladen, sichtbar, abgelöst, nackt) und gibt es verborgen; das Feld
  zeigt es. `nimmPanelZurueck()` holt es nach dem Freigeben in die
  Fläche und, mit Ziel, als Fenster dorthin. Die Profilwerte
  (`contestPanels`) führen ein Panel im Feld nicht als Fenster;
  `stelleAnordnungHer`/`raeumeAnordnung` lassen es im Feld.
- **Profil:** im `kombiInhalt` steht es als `Contest:<id>` (Vorsatz,
  damit „chat" nicht wie ein Applet aussieht); das Zurückholen kennt
  den Vorsatz; das Leeren vor Schritt 1 gibt ein Contest-Panel an das
  Logbuch zurück, und das neue Profil sagt dann selbst, was es will.
- **Herausziehen** aus dem Feld: `gibFremdesPanelZurueck` erkennt ein
  Contest-Panel und macht daraus ein Fenster am Logbuch, dort, wo
  losgelassen wurde — keine Longpath-Hülle.

Prüfstände: `tst_contestfenster::einPanelGehtInEinFremdesGehaeuseUndZurueck`,
`tst_contest_panels_im_containers_menu::dasCombinedFeldNimmtEinContestPanel`
(gegen die Fassung ohne die vier Zweige rot), Contestprogramm
`test_einbettung::imFremdenGehaeuseIstDasPanelNackt`.
