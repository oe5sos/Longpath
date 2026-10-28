# Begleitgerät und Transverter, Stufe 2

2026-10-28. Baut auf Stufe 1 (PR #337) auf, die den CAT-Client, die
Transverter-Rechnung und die Einstellseite brachte — und drei Lücken
ausdrücklich offen ließ.

Diese Stufe schließt alle drei.

## Was Stufe 1 offen ließ

1. **Die Dauerverbindung.** Der CAT-Client saß in der Einstellseite.
   Eine `SetupPage` wird bei jedem Öffnen neu gebaut und beim
   Schließen abgeräumt — die Verbindung war beim ersten Blick auf eine
   andere Seite weg.
2. **Die Marke im Panadapter** samt Klick-Abstimmung.
3. **Der Versatz in Anzeige, Bandplan und Log.** Die Rechnung stand und
   war geprüft, angewandt wurde sie nur auf der Einstellseite selbst.

## 1. Die Verbindung gehört nicht in das Fenster

`CompanionRigController` (`src/core/CompanionRigController.{h,cpp}`)
besitzt jetzt `RigctldProcess` und `RigctldClient`, und `RadioModel`
besitzt ihn. Angelegt im Ctor, **nicht** verbunden — verbunden wird aus
`MainWindow::verdrahteBegleitgeraet()`, nachdem die Oberfläche steht.

Der Unterschied zum Rotor eine Zeile darüber in `RadioModel.h` ist
Absicht und steht dort auch so: beim Rotor wurde der kleine, umkehrbare
Schritt gewählt (eine Registrierung statt Besitz), weil er am Mast des
Betreibers hängt und eine Besitzumstellung eine Architekturänderung an
einem laufenden Gerät gewesen wäre. Das Begleitgerät ist neu und hängt
an nichts.

Die Einstellseite **bedient** jetzt diese eine Verbindung, statt eine
zweite aufzumachen. Zwei Clients an einem rigctld wären nicht nur
doppelt, sondern falsch: es antwortet in der Reihenfolge der Befehle,
nicht nach Absender, und beide bekämen Teile der Antworten des anderen.
Ohne Modell (der Prüfstand baut die Seite nackt) bleibt der alte Weg mit
eigenem Client — so lässt sich die Seite ohne halbes Programm prüfen.

### Zwei Dinge, die hier nicht passieren

**Getastet wird nicht.** `RigctldClient` kann es nicht, und dieser
Controller fügt es nicht hinzu. Er liest PTT, er setzt es nie.

**Die Frequenz des fremden Geräts führt Longpaths eigene Scheibe
nicht.** Das wäre ein Kreis, sobald der Klick in die andere Richtung
geht: die Scheibe schickte dem Gerät eine Frequenz, das Gerät meldete
sie zurück, und beide liefen davon. Das Gerät treibt die **Marke**,
nichts weiter.

### Longpath darf nicht sich selbst befragen

Longpath hat einen **eigenen** rigctld-Server (Setup → CAT → TCP/IP
CAT), damit Fremdprogramme es wie ein Funkgerät steuern können. Der
bindet nach Vorgabe `127.0.0.1:4532` — und 4532 ist auch die Vorgabe,
die im Feld des Begleitgeräts steht.

Wer beides einschaltet und nichts umstellt, baut einen Kreis: Longpath
fragt Longpath nach der Frequenz, bekommt seine eigene, die Marke klebt
auf der eigenen Scheibe, und ein Klick im Panadapter schickt ein `F` an
das eigene Programm. Nichts stürzt ab, nichts warnt, und es sieht so
lange richtig aus, bis der Betreiber merkt, dass das Funkgerät sich nie
bewegt.

`zieltAufUnsSelbst()` fängt das und sagt, was zu tun ist. Geprüft wird
gegen die **Einstellung**, nicht gegen einen laufenden Server: der
Controller kennt `MainWindow` nicht und soll ihn nicht kennen müssen.
Fällt die Einstellung anders aus als die Wirklichkeit, verweigert die
Prüfung einmal zu oft — die harmlose Richtung.

## 2. Die Marke sitzt in der ZF, die Zahl daran auf der Antenne

Das ist der Punkt, an dem man es falsch machen kann, und zwar so, dass
es lange richtig aussieht. Gibt man der Marke eine Antennenfrequenz,
landet sie 116 MHz neben dem Signal — also außerhalb des Fensters und
damit gar nicht im Bild. Der Betreiber liest das als „die Marke kommt
nicht", sucht in der Verbindung, und die ist in Ordnung.

`SpectrumWidget::setCompanionMarker(bool, qint64 ifHz, QString label)`
nimmt darum ausdrücklich eine **ZF**-Frequenz. Gemalt wird eine
senkrechte Strichlinie und ein kurzes Schild mit der **Antennen**zahl.
Warm (`kAmberText`), nicht blau: der Hausstil hält Blau für das
Anfassbare, und diese Zahl ist gemessen, nicht gestellt. Kein Rot — es
ist keine Warnung.

Das Schild kippt nach links, wenn rechts kein Platz mehr ist.

**In beiden Malwegen.** `SpectrumWidget` hat einen QPainter-Weg und
einen QRhi-Überlagerungsweg, und der Kommentar an der zweiten Stelle
warnt genau davor: der erste ist die Rückfallebene ohne GPU, der zweite
malt im Betrieb. Eine Einblendung in nur einem von beiden ist unsichtbar
oder erscheint nur, wenn keine Grafikkarte da ist.

### Der Klick geht in beide Richtungen

Kein neues Signal und keine neue Mauslogik in einem 13 000-Zeilen-Widget:
`frequencyClicked(double hz)` gab es schon, und `hz` ist eine Frequenz
auf **dieser** Achse, also die ZF — genau, was das Gerät will. Der
Handler in `MainWindow::wireSpectrumSliceControls` stimmt jetzt beides
ab: Longpaths eigene Scheibe und, wenn eingeschaltet, das fremde Gerät.

Nur dort, nicht bei jeder Frequenzänderung der Scheibe. Ein Band- oder
Speicherwechsel, ein Drehknopf, ein Spot: das alles bewegt die Scheibe
ebenfalls, und würde man das weiterschicken, liefen Programm und Gerät
gegeneinander. Ein Klick ist eine Ansage des Bedieners, kein Zustand.

## 3. Der Versatz, an vier Stellen

Den Versatz braucht nicht ein Ort, sondern vier: die Zahl in der
VFO-Anzeige, die Frequenzachse im Panadapter, die Bandplan-Abschnitte
darin und der Logbucheintrag. Alle vier liegen weit auseinander —
`VfoDisplayItem` entsteht tief im Instrumentensystem (`ItemGroup`), wo es
keinen Weg gibt, ihm beim Anlegen etwas mitzugeben, ohne die
Eigenschaftsmasken und die Speicherung mit anzufassen.

Darum eine **prozessweite Fassung**, wie `AppSettings::instance()` es
vormacht: eine Stationseinstellung, von vielen gelesen, von genau einer
Stelle geschrieben (`CompanionRigController`, nach dem Laden und nach
jedem Speichern des Bedieners). Nur vom Hauptfaden aus — alle vier Leser
malen oder schreiben Logbuch, und beides läuft dort.

Absichtlich **nicht** aus `load()` vorbelegt: eine Vorbelegung, die beim
ersten Zugriff die Einstellungsdatei liest, hängt jeden Prüfstand, der
eine Zahl malt, an eine Datei — und in einem Lauf ohne Einstellungen
entscheidet die Aufrufreihenfolge, ob der Versatz greift.

### Aus heißt aus

Das ist hier die Hauptsache und nicht die Formalie. Diese Änderung greift
in den Malcode des Panadapters, in die große VFO-Zahl und in das
Logbuch eines Programms, mit dem der Betreiber Contest fährt. Eine
Umrechnung, die „eigentlich nichts tut", tut im Zweifel doch etwas: ein
`llround` hier, ein verschobenes Bandplanfenster dort.

Darum ist jede Umrechnung so gebaut, dass der Zweig bei
ausgeschaltetem Transverter **nicht betreten** wird und bitgleich
dieselbe Zahl zurückkommt, die hereinkam. Die Gegenprobe zeigt, was
sonst passiert: ohne den aus-Zweig wird aus `28300000.4` ein
`28300000`.

Und `logbuchFrequenz()` gibt `false` zurück, wenn kein Transverter im
Spiel ist. Dann bleiben Frequenz und Band des Aufrufers unberührt und
seine eigene Rechnung steht — das ist der Zweck dieser Form.

### Das Band

Longpaths Band-Aufzählung endet bei 6 m (darum das ZF-Fenster und kein
Bandpaar, siehe Stufe 1). `bandFromFrequency(144300000)` hat also keinen
Eintrag — und ein ADIF-Satz ohne `BAND` oder mit `BAND=10M` bei einer
144-MHz-Frequenz ist falsch, nicht nur unschön: der Satz wandert in ein
Diplom-Programm.

`TransverterSetup::adifBand()` ist deshalb eine kleine, geschlossene
Tabelle der ADIF-Bandnamen — **keine** neue Bandverwaltung für Longpath.
Sie wird nur betreten, wenn ein Transverter aktiv ist, sie entscheidet
nichts über das Senden, und ihre Grenzen sind die der
ADIF-Spezifikation, nicht die des österreichischen Bandplans: ein
Austauschformat richtet sich nicht nach dem Land.

Fällt kein Bandname heraus, bleibt das Feld des Aufrufers stehen. Ein
falsches Band ist schlimmer als ein ungenaues.

### Das Schild an der VFO-Zahl

Die große Zahl zeigt mit Transverter die **Antennen**frequenz. Longpaths
eigener Sender steht dabei auf der ZF, und `BandPlanGuard` — die
Autorität für das Senden — sieht ebenfalls die ZF. Das ist richtig so:
der Wächter bewacht Longpaths Sender, nicht den des fremden Geräts, und
er wird hier **nicht angefasst**.

Nur darf der Betreiber dann nicht „144.300" lesen und glauben, Longpath
sende dort. Darum ein kurzes, warmes `TRV` neben der Zahl: es sagt, dass
sie umgerechnet ist.

Das Rad zum Abstimmen (`digitWeightAt`) bleibt unberührt und bleibt
richtig: der Versatz verschiebt um ganze MHz, die kHz- und Hz-Stellen
decken sich also, und eine MHz-Stelle bewegt die Anzeige um genau so
viel wie den Empfänger.

## Prüfstände

Drei neue, 46 Fälle:

- `tst_begleitgeraet_regler` (17) — nicht von selbst verbinden, nicht
  sich selbst befragen, der Versatz beim Laden, kein Abstimmen ohne
  Verbindung.
- `tst_transverter_anzeige` (15) — der Versatz dort, wo er vor dem
  Betreiber landet; vor allem die Gleichheit ohne Transverter.
- `tst_begleitgeraet_marke` (14) — wo die Marke sitzt (in der ZF) und
  was daran steht (auf der Antenne).

**Vier Gegenproben, vier Mal rot:**

| Entfernt | Was umfällt |
|---|---|
| der aus-Zweig in `rfFuerAnzeige` | `28300000.4` wird `28300000` |
| die Marke nimmt RF statt ZF | Marke verschwindet aus dem Bild |
| `zieltAufUnsSelbst()` | kein Grund gemeldet, stumme Selbstbefragung |
| `active()` in `logbuchFrequenz` | das Logbuch wird ohne Transverter angefasst |

## Ein Befund außerhalb dieser Änderung

**Kein Prüfstand ist netzisoliert.** `AppSettings::isSandbox()` hängt
allein an `LONGPATH_CONFIG_DIR`; `TestSandboxInit` setzt es nicht (es
schaltet `QStandardPaths::setTestModeEnabled` ein), und
`tools/run_tests.sh` setzt es auch nicht. In einem gewöhnlichen
`ctest`-Lauf ist `darfVonSelbstVerbinden()` daher **wahr** — jedes
Prüfprogramm darf ins Netz.

Gefunden hat das die Voraussetzungsprüfung in
`inDerSondeVerbindetNichtVonSelbst()`: ohne sie wäre der Fall grün
gewesen, **weil** die Voraussetzung ausfiel. Er hätte dann nichts
geprüft und das stillschweigend getan.

Der Prüfstand stellt die Bedingung jetzt selbst her (`qputenv` in
`initTestCase`). Dass sie im ganzen Baum fehlt, ist ein eigener Befund
und eine eigene Änderung: sie betrifft 539 Prüfstände und verschiebt für
alle den Einstellungsordner.

## Live geprüft, nicht nur im Prüfstand

`tools/begleitgeraet_probe.cpp` (Werkzeug, `EXCLUDE_FROM_ALL`) fährt die
ganze Kette gegen ein **echtes** rigctld — Hamlibs Dummy-Gerät
(Modell 1) auf Port 4599, offscreen, in einem eigenen
Einstellungsordner. Das fängt, was ein selbstgebauter Server nicht
fängt: Zeilenenden, Reihenfolge, Begrüßung, die Form der
RPRT-Antworten. Dieselbe Lehre wie beim Sender gegen den eigenen
Empfänger.

Der Lauf belegt:

- `starteWennGewuenscht()` verbindet in einem isolierten Lauf **nicht**,
  auch mit `CompanionRigEnabled=True`.
- Von Hand verbindet es, liest die Frequenz, und `stimmeAbZf()` kommt
  am Gerät an — die Gegenprobe ist, dass das Gerät die neue Frequenz
  zurückmeldet.
- Beiläufig die wichtigste Regel: das Dummy-Gerät meldete
  **145,000 MHz**, außerhalb des 28–30-MHz-Fensters. `rfFrequenzHz()`
  gab 145,000 zurück, nicht 261 MHz. Die Fensterprüfung greift also
  nicht nur im Prüfstand, sondern an einer Zahl, die kein Prüfstand
  gewählt hat.

Dazu zwei Bilder aus dem echten Malcode: die VFO-Zahl mit dem
TRV-Schild (Longpath auf 28,300 MHz zeigt `144.300.000`) und die Marke
im Panadapter.

Port 4599, nicht 4532: dort könnte Longpaths eigener rigctld-Server
stehen, und dann befragte die Sonde das Programm statt das Gerät.

## Was weiter offen ist

- **Alles am echten Gerät.** Baudrate, PTT-Quelle, Anschlussname, und
  ob die Marke bei laufendem Funkbetrieb ruhig steht.
- **Die Hardware-Entscheidung** — TS-590S oder K3.
- Der Transverter-Versatz wirkt nicht in den **Spots** und nicht in der
  **Bandkarte**. Beide zeigen bei aktivem Transverter weiter die ZF.
  Bewusst ausgelassen: ein Spot auf 28,3 MHz, der als 144,3 MHz
  erscheint, wäre falsch, solange die Spotquelle selbst nichts von einem
  Transverter weiß.
