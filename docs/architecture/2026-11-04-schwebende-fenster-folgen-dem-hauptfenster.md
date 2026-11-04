# Schwebende Fenster folgen dem Hauptfenster; das Logbuch liegt auf ihrer Ebene

2026-11-04. Zwei Wünsche des Betreibers vom selben Vormittag, beide an
derselben Stelle: Longpaths schwebende Fenster (abgelöste Applets,
schwebender Panadapter, Rotor/Log, Seitenfläche) sind `Qt::Tool`-Kinder
des Hauptfensters — auf macOS NSPanels auf der schwebenden Ebene.

## 1. „wenn ich contestprogramm öffne, soll das nach anklicken im vordergrund bleiben!"

Das Contest-Logbuch (seit #381 als Fenster in Longpath) war ein
gewöhnliches, elternloses `QMainWindow`. Ein gewöhnliches Fenster liegt
auf macOS IMMER unter einem NSPanel der schwebenden Ebene derselben
Anwendung, egal wer zuletzt geklickt wurde. Der Betreiber sah: Bandwidth
Filter, Panadapter, S-Meter … über dem Log, ein Klick brachte es nur
unter den normalen Fenstern nach vorn.

Behebung in `ContestFenster::oeffne(grund, elternFenster)`: mit
Longpaths Hauptfenster als Eltern wird das Logbuch ein `Qt::Tool` — DIE
Ebene der Panels. Auf einer Ebene gilt die Klickreihenfolge. Dazu
`WA_MacAlwaysShowToolWindow`, sonst verschwände ein Werkzeugfenster,
sobald Longpath nicht mehr vorn ist (für ein Logbuch falsch: es soll
stehen bleiben, auch wenn man kurz im Browser ist). Die Knöpfe des
Rahmens bleiben, nur der Fenstertyp wechselt. Ohne Elternfenster
(Prüfstand, Eigenständigkeit) bleibt es ein gewöhnliches Fenster.

Folge: `legeSchwebendeFensterInsDock()` nimmt das Logbuch beim
Minimieren mit — bewusst NICHT über `versteckeSchwebendeFenster()`,
denn hinter die Connect-Maske soll es nicht: ein Funkgerätewechsel
mitten im Contest darf das Log nicht wegräumen.

Prüfstand: `tst_contestfenster::mitElternfensterLiegtEsAufDerEbeneDerPanels`.

## 2. „sobald ich das _ oder das + usw. links oben verwende, muss sich das ganze Fenster bewegen! auch die freischwebenden im verhältnis!"

„_" ist seit #379 erledigt (Dock-Ablage nimmt die Fenster mit). „+"
(Zoom), Vollbild und ein Zug am Titelbalken änderten den Rahmen des
Hauptfensters — und die schwebenden blieben stehen, wo sie waren.

`MainWindow::schwebendeMitziehen()`: bildet jedes schwebende Fenster
(dieselbe Aufzählung wie `schwebendeAusDemSchutzrahmenHolen`: sichtbare
Fensterkinder des Hauptfensters, keine Dialoge, nichts Modales) von
seiner Lage IM VERHÄLTNIS zum alten Rahmen auf den neuen ab:

    x' = neu.x + (x − alt.x) · neu.breite / alt.breite   (y ebenso)

Ein reiner Zug (gleiche Größe) wird so zum gleichen Versatz, eine
Größenänderung hält die Lage im Verhältnis; die GRÖSSE der schwebenden
Fenster bleibt dem Betreiber (`move()`, nicht `setGeometry()`). Was
dabei vom Schirm fiele, wird am Schirmrand gehalten. Läuft im Nachlauf
des Schutzrahmens (250 ms nach dem letzten move/resize), VOR dem
Herausholen aus den Sperrstreifen — wer mitgezogen auf einer
Bedienleiste landet, wird gleich noch heruntergeschoben.

`m_hauptrahmenBasis` ist der Rahmen, zu dem die Lagen zuletzt passten.
Eine EIGENE Bewegung des Hauptfensters — Profil anwenden
(`LayoutProfiles::runApply` läuft unter `EigeneBewegung`), der Aufbau
(`m_aufbauBewegung`, `showMaximized()` beim Start), das Raster — setzt
nur die Basis neu und zieht nichts: dort stellt das Profil die
schwebenden Fenster ohnehin selbst. Dafür neu: `eigeneBewegungLebt()`
in WindowPlacement. Das Logbuch ist seit Teil 1 Teil dieser Aufzählung
und zieht mit.

Gezogen wird nur, wenn im laufenden Nachlauf eine FREMDE Bewegung war
(`m_fremdeBewegungAnhaengig`). Gefunden am Gerät, 12:09:43, erster Start
mit dieser Fassung: „Hauptfenster QRect(0,48 …) → QRect(0,33 …): 5
schwebende Fenster mitgezogen" — Betreiber: „wieder irgendeine
zusammensetzung der panels". Das Raster hatte das frisch maximierte
Hauptfenster um 15 px nachgerückt; der Sprung war klein und der
Nachlauf lief, also galt er als Nachhall einer fremden Bewegung, die es
nie gab. Beim Aufbau ist jede Bewegung eigen. Das wiegt doppelt, weil
`captureIntoCurrent()` beim Beenden die aktuelle Anordnung ins Profil
schreibt: ein Versatz bei jedem Start wandert so ins Profil.

Prüfstand `tst_schwebende_folgen_dem_hauptfenster`: Zug → gleicher
Versatz; Wachsen → Verhältnis gehalten, Größe unverändert; eigene
Bewegung NEBEN dem Raster (−93,−41; das Raster rückt danach nach) →
nichts. Alte Fassung: die ersten beiden Fälle fallen um.

## 3. „jedes panel vom contestprogramm sollte in longpath eigens hinzuzufügen sein"

Betreiber, später am selben Tag, mit dem Tools-Menü auf dem Bild: „am
besten wäre, wenn hier alle auch vom logprogramm contestprogramm
stehen" — und: „wir haben dann auf der einen seite das eigene contest
programm, wenn es probleme gibt und wenn ich will, kann ich dies eigens
mit longpath nutzen."

Beobachtet am Gerät: ist das Logbuch das aktive Fenster, zeigt die
Menüleiste nur „Apple" und „Longpath" — weder Longpaths Menüs noch das
Fenster-Menü des Contestprogramms (dort sitzt „Als eigenes Fenster").
Darum liegt die Liste in Longpaths Tools-Menü: **Tools › Contest
logbook panels**, je Panel ein Haken „als eigenes Fenster", dazu „alle"
und „alle zurück". Gefüllt beim Aufklappen, weil sich der Stand im
Logbuch selbst ändert (✕ am Panel dockt es zurück).

* Contestprogramm (oe5sos/Contestprogramm#72): `MainWindow::panelEintraege()`
  (die Tabelle von Fenster › Panels, jetzt an EINER Stelle),
  `istPanelSchwebend`, `setzePanelSchwebend` (holt ein Panel vorher aus
  dem Seitenbereich; ein nie gezeigtes Hauptfenster hat keine ausgelegte
  Fläche, dann bekommt das Panel die Größe des Entwurfs), `panelFenster`.
  Eigenständig ändert sich nichts.
* Longpath: `ContestFenster::lade()` — Controller, Datenbank, Fenster
  OHNE es zu zeigen; `panels()`, `zeigePanelAlsFenster()` (lädt bei
  Bedarf verborgen; ein Panel aus einem nie gezeigten Logbuch wird neben
  das Hauptfenster gestellt, gestaffelt), `dockePanel()`, `panelFenster()`.
  Das Panel ist ein `Qt::Tool`-Kind des (verborgenen) Logbuch-Fensters,
  das am Longpath-Hauptfenster hängt — also auf der Ebene von Longpaths
  eigenen Panels.
* Die Aufzählung der schwebenden Fenster (Mitziehen, Schutzrahmen,
  Dock-Ablage) geht jetzt über die Fensterkette (`schwebtUnter`): ein
  Fenster gehört dazu, wenn es über beliebig viele Fensterstufen am
  Hauptfenster hängt. Vorher nur direkte Kinder.

Prüfstand: `tst_contestfenster::einPanelOhneLogbuchFenster` (Panel kommt
als Tool-Fenster mit Größe neben das Elternfenster, zweites gestaffelt,
Logbuch bleibt zu, zurück, dann Logbuch zeigen = dasselbe Fenster);
Contestprogramm `test_einbettung::panelsLassenSichVonAussenAbloesen`.

Offen: macOS zeigt ein Werkzeugfenster, dessen Elternfenster verborgen
ist — das ist am Gerät zu sehen, nicht im offscreen-Prüfstand.

## 4. „alle panels haben hier noch runde kanten, dies bitte auf eckig umstellen" — und die Rotoren klein

Die Panels des Contestprogramms kamen als `Qt::Tool`-Fenster mit dem
Rahmen des Systems: Titelleiste, Ampelknöpfe, runde Ecken. Longpaths
eigene Fenster (`ToolWindow`, `AppletFloatingWindow`, `PanFloatingWindow`)
sind `Qt::Tool | FramelessWindowHint` mit eigener Kopfzeile — eckig.
Also (oe5sos/Contestprogramm#72, f432e0a):
`PanelContainerWidget::setzeRahmenlosSchwebend(true)`, von
`ContestFenster::lade()` gesetzt, wenn ein Elternfenster da ist: ein
schwebendes Panel wird rahmenlos, der Panelrahmen eckig (Radius 0), die
Kopfzeile bekommt den Rückweg ↙ (das ✕ des Systems fällt weg; chromlose
Panels wie die Karte einen Knopf neben dem Schloss), Zug an der
Kopfzeile und Anfasser rechts unten arbeiten wie in der Fläche, geklemmt
an den Schirm statt ans Elternfenster. Eigenständig bleibt alles beim
Systemrahmen.

„das fenster rotoren muss sehr groß gezogen werden, um grad eigens zu
sehen. bitte um lösungen, um fenster klein ziehen zu können": die
Rotorzeile war ein QHBoxLayout, Mindestbreite = Summe beider Rotoren,
und am Mindestmaß fielen die Gradzahlen weg (r ≤ 84). Jetzt
`RotorRowWidget` (Mindestbreite = EIN Rotor; nebeneinander, wenn die
Breite reicht, sonst untereinander) und Gradzahlen bis r = 54 eine Stufe
kleiner statt weg.

Nachtrag (oe5sos/Contestprogramm#72, 8f0ac07): „die kästchen für die
gradanzahl bei rotoren können kleiner sein, so wäre dann alles auch
gemeinsam kleiner" — das Ablesefeld unter dem Zifferblatt (Aktuell /
Ziel / Entfernung) hat jetzt drei Stufen nach der Breite (38 / 28 / 16 px
Wertschrift), und am Mindestmaß ist es da, klein, statt weg. Und „bei
karte und verbindungen sollten die rotoren eher mittig sein": die
Rotorscheiben in der Kartenspalte stehen in der Spaltenmitte.

## Was bewusst NICHT passiert

* Kein Mitziehen bei jedem Pixel eines Größenzugs — Nachlauf, wie beim
  Schutzrahmen. Fenster, die im Takt springen, sind unbedienbar.
* Keine Größenänderung der schwebenden Fenster „im Verhältnis". Der
  Betreiber sagte Lage, und ein S-Meter, das mit dem Hauptfenster
  wächst, wäre ein anderes Fenster.
* Nichts für Fenster, die gerade versteckt sind (Dock, Connect-Maske):
  sie stehen nach dem Zurückholen, wo sie waren.
