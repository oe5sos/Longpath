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

Prüfstand `tst_schwebende_folgen_dem_hauptfenster`: Zug → gleicher
Versatz; Wachsen → Verhältnis gehalten, Größe unverändert; eigene
Bewegung → nichts. Alte Fassung: die ersten beiden Fälle fallen um.

## Was bewusst NICHT passiert

* Kein Mitziehen bei jedem Pixel eines Größenzugs — Nachlauf, wie beim
  Schutzrahmen. Fenster, die im Takt springen, sind unbedienbar.
* Keine Größenänderung der schwebenden Fenster „im Verhältnis". Der
  Betreiber sagte Lage, und ein S-Meter, das mit dem Hauptfenster
  wächst, wäre ein anderes Fenster.
* Nichts für Fenster, die gerade versteckt sind (Dock, Connect-Maske):
  sie stehen nach dem Zurückholen, wo sie waren.
