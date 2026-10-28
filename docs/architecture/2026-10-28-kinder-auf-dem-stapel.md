# Kinder auf dem Stapel — die zweite Hälfte

2026-10-28. Aus der Durchsicht des Upstream-Rückstands
(`longpath-upstream-rueckstand-2026-10-27`).

## Woher der Anlass kam

NereusSDR hat am 22.10. vier Commits zu genau diesem Thema gefahren
(`f26e88471`, `cf6d1b63e`, `8461be26b`, `ea0ade1bf`). Drei davon sind
**Portierungen unseres eigenen Codes zurück zu ihnen** — sie sagen es
selbst: *„Port Longpath's fix (PR 61, 54f6e876)"*, *„Ported from
Longpath PR 83"*. Da war nichts zu holen.

Aber einer ging weiter als wir: `cf6d1b63e` stellte **31 Dialoge** um,
nicht nur die Menüs. Und `8461be26b` fand Menüs auf dem Haufen, die
nach `exec()` von Hand freigegeben wurden.

Beides hatten wir noch.

## Der Fehler, zur Erinnerung

Ein Qt-Kind auf dem Stapel dreht in `exec()` seine eigene
Ereignisschleife. Stirbt das Elternteil darin — ein Fenster wird
geschlossen, das Programm beendet —, räumt Qt das Kind als sein Kind
ab. Beim Verlassen des Rahmens wird es **ein zweites Mal** freigegeben:
`pointer being freed was not allocated`, Abbruch.

Am 2026-08-30 ist genau das passiert, mit einem offenen Kontextmenü.
Die Menüs wurden am 2026-09-22 auf `ScopedChildWidget` umgestellt
(`src/gui/ScopedChildWidget.h`).

**Die Dialoge nicht.** Sechs `QMessageBox` standen heute noch auf dem
Stapel, jede mit Elternteil:

| Datei | Was danach noch angefasst wird |
|---|---|
| `src/gui/MainWindow.cpp` | `clickedButton()`, dann `restartNow()` auf `this` |
| `src/gui/SupportDialog.cpp` | `clickedButton()` zweimal |
| `src/gui/setup/AudioAdvancedPage.cpp` | `clickedButton()` |
| `src/gui/setup/AudioVaxPage.cpp` | die ganze `parent()`-Kette |
| `src/gui/setup/hardware/AntennaAlexAlex1Tab.cpp` | `m_hpfBypassOnPs` |
| `src/gui/setup/TxProfileSetupPage.cpp` | nichts — der Rahmenausgang allein genügt |

## Die andere Richtung: Menüs auf dem Haufen

Sechs Stellen bauen ihr Kontextmenü mit `new QMenu(window())` und geben
es nach `exec()` wieder frei. Fünf mit `deleteLater()`, eine mit
`delete`. **Beides ist derselbe Fehler**, nur andersherum: ist das
Fenster in `exec()` gestorben, hat Qt das Menü schon geholt, und der
Zeiger in der Hand zeigt ins Leere. `deleteLater()` schützt nicht — es
ist ein Methodenaufruf auf einem freigegebenen Objekt.

`PanadapterApplet`, `InstrumentApplet` (zweimal), `TunerApplet`,
`AmpApplet`, `Rf2ksApplet`.

## Was geändert wurde

**`ScopedChildWidget` bekommt einen weiterleitenden Konstruktor.**
Bisher nahm er nur `(QWidget* parent)`. Ein `QMessageBox(Icon, Titel,
Text, Knöpfe, parent)` ging damit nicht durch den Halter — und genau
diese Form steht in `AntennaAlexAlex1Tab`. Jetzt reicht er durch, was
der Aufrufer angibt:

```cpp
template <typename... Args>
    requires std::constructible_from<Widget, Args...>
explicit ScopedChildWidget(Args&&... args)
    : m_widget(new Widget(std::forward<Args>(args)...))
```

Die `requires`-Klausel hält den Kopierkonstruktor heraus: ein `QMenu`
lässt sich nicht aus einem `ScopedChildWidget` bauen, also greift die
Vorlage dort nicht und das gelöschte Kopieren bleibt stehen.
(AetherSDR hat dieselbe Änderung in `9befcd24` gemacht; NereusSDR zog
sie in `cf6d1b63e` nach.)

**Die sechs Dialoge gehen durch den Halter**, und wo nach `exec()` noch
etwas am Elternteil oder am Dialog hängt, steht davor `if (!dlgOwner)
{ return; }`.

**Die sechs Menüs liegen in einem `QPointer`** und werden nur
freigegeben, wenn es sie noch gibt. Inhalt, Reihenfolge und Aktionen
sind unverändert.

## Festgenagelt

`tests/tst_kein_kind_auf_dem_stapel.cpp` liest die Quellen und hält
beide Decken auf **null**: kein Dialog dieser Art auf dem Stapel, kein
Menü von Hand freigegeben. Dazu eine dritte Probe, die prüft, dass der
Lauf die Quellen wirklich gelesen hat — ein Lauf, der grün ist, *weil*
er nichts fand, ist kein Lauf (dieselbe Lehre wie am Vormittag bei
`tst_sonde_bleibt_offline`).

`tests/tst_scoped_child_widget.cpp` bekommt zwei Fälle für den
weiterleitenden Konstruktor: dass er ein mehrstelliges Kind baut, und
dass er es genauso schützt.

## Was das nicht ist

Keine Verhaltensänderung. Kein Text, kein Knopf, keine Vorgabe und
keine Reihenfolge wandert. Nur der Besitz.

Und es ist nicht vollständig in dem Sinn, dass jetzt kein Qt-Kind mehr
auf dem Stapel stünde — die Decke deckt die sechs Dialogtypen ab, die
`exec()` haben und ein Elternteil nehmen. Ein `QDrag` oder ein eigener
`QDialog`-Abkömmling fällt nicht darunter; NereusSDR hat für den
`QDrag` des Container-Umbaus eine eigene Stelle. Bei uns steht dort
nichts Vergleichbares.

## Was die Durchsicht sonst ergab

Gefiltert wurde nach dem Verfahren aus dem Gedächtnisblatt: 212
Commits bei `upstream/main` seit dem 22.10., davon **134 mit einer
Änderung unter `src/`**. Der Rest ist iPhone/iPad, PathRacer,
NSDC-Fernbedienung und Textkosmetik — alles Dinge, die wir nicht
haben.

Von den acht Themen des Gedächtnisblatts ist eines hiermit erledigt
(es war keines davon — die Abbau-Abstürze kamen erst beim Lesen dazu).
Die Lage der übrigen:

| Thema | Lage |
|---|---|
| Spektrum/Wasserfall bei Paketverlust | **besetzt** — PR #338 der Parallelsitzung, inzwischen auf main. `865ed0eee` ist ohnehin *unser* Auslöser, zu ihnen portiert |
| SWR-Schutz (`217bc525b`, `d8ee2c8ad`, `f97f338cc`) | der Hauptbefund („der Faktor wurde nirgends angewandt") gilt bei uns **nicht** — `computeWireDrive` multipliziert ihn seit je ein, und `d8ee2c8ad` nennt unseren Commit `99b20c81` namentlich. Offen bleibt die Thetis-Treue bei Freigabe auf jedem Unkey und beim Riegel nach offener Antenne. **Lohnt eine eigene Runde** |
| WDSP-Eingangsschutz (`380b64189`, `8b38b227a`) | klein und hart; nicht geprüft |
| CTUN (`f183631b9`, `26424b72c`, `5d071cd64`) | nicht geprüft |
| Filter-Vorgaben gegen Thetis | sechs Commits, teils Dinge, die wir nicht gebaut haben |
| Antennentabelle | drei Commits; berührt `AlexController` |
| AGC/RF-Gain | drei Commits |
| NB Blank / Rauschminderung | vier Commits |
| Gerätesuche (`e301246ea`, `a4d3b4368`, `ea7a391fd`) | **neu dazu**; wir hatten am 16.10. „Gerätesuche blockierte den Start" |
| MaxBin (`5e6bef59d`, `ddaa76919`) | **neu dazu**; betrifft den S-Meter |

Nicht zu holen, weil es unser eigener Code ist: `b1a1eeff2`,
`865ed0eee`, `f26e88471`, `cf6d1b63e` (im Kern), `ea0ade1bf`.
