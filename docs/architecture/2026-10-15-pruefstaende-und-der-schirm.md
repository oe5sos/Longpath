# Prüfläufe, die vom Schirm abhängen

*2026-10-15 — Martin Fischer (OE5SOS), KI-gestützt über Anthropic Claude*

## Der Befund

Ein voller `ctest`-Lauf auf dem MacBook meldete drei rote Läufe. In der
CI waren dieselben Läufe **grün — auf Linux UND auf macOS**. Die
Ursache war in allen drei Fällen dieselbe Familie: der Prüfstand hängt
an der Darstellungsumgebung (Bildschirmgröße, Bildpunktdichte,
Fensterlage), und die CI-Läufer haben eine andere.

Das ist die unangenehmste Sorte Fehlalarm: er trifft genau dort, wo
gearbeitet wird, und ist genau dort unsichtbar, wo das Tor steht.

## Die drei Fälle, gemessen

### 1. `tst_real_menus_hang_on_the_window` — hing endlos

Der Lauf öffnet 21 Menüs und schloss sie über
`QTimer::singleShot(200, …)` auf `QApplication::activePopupWidget()`.
Das trug 19 davon. Bei zweien lief er in CTests 120-Sekunden-Grenze —
**seit Wochen kam er auf einem Mac nie bis zum Ende.**

Mit `sample` am hängenden Prozess gemessen, zwei Gründe:

1. `QMenu::exec()` dreht auf macOS eine Cocoa-Schleife
   (`-[NSApplication run]` → `_BlockUntilNextEventMatchingListInMode`).
   Darin kommen weder Qt-Taktgeber noch gepostete Ereignisse
   zuverlässig an. Beides ausprobiert, beides wirkungslos.
2. Zwei dieser Menüs bekommen im Lauf die Höhe **null**
   (`QRect(1299,1556 166x0) outside any known screen`) — sie werden nie
   sichtbar und damit nie `activePopupWidget()`. Auch ein
   funktionierender Taktgeber hätte sie nicht gefunden.

**Behoben im Prüfstand, nicht im Programm:** ein Ereignisfilter auf
`QEvent::Show` läuft synchron in `sendEvent`, also während `exec()` das
Menü zeigt und *bevor* es in die Schleife eintritt. Von dort wird das
Schließen **gepostet**; beim Eintritt in die Schleife arbeitet Qt die
eigene Warteschlange ab.

Wichtig für den nächsten Anlauf: ein zusätzliches *direktes* `close()`
im Show-Handler bricht die vorher laufenden Fälle. Nur die gepostete
Schließung.

Vorher: endlos (dreimal gemessen, auch allein gestartet).
Nachher: **8 Fälle, 7,4 Sekunden.**

Drei Wege wurden probiert und wieder verworfen: `popup()` statt
`exec()` im `QsoRecorderApplet` (bringt diesen einen Fall durch, dann
hängt der nächste), gepostete Ereignisse statt Taktgeber (kommen in der
Cocoa-Schleife nicht an), Filter mit direktem `close()` (bricht
Laufende).

### 2. `tst_command_bar` — zählte Bildpunkte in Layoutpunkten

`zwischenDenGruppenStehtEinStrich` fand 2 von 5 Strichen.

Gemessen: die Leiste ist 1400×70 **Layoutpunkte**, `grab()` liefert auf
einem Retina-Schirm ein Bild von **2800×152 Bildpunkten**. Der Lauf
suchte die Striche an den Layout-Koordinaten im Bild — also an den
falschen Stellen, und fand zufällig zwei.

**Behoben:** `bild.devicePixelRatio()` einrechnen. Lücken ohne Platz
werden nicht mehr mitgezählt.

Gegenprobe: mit `dpr = 1.0` fällt der Lauf wieder auf 2 von 5.

### 3. `tst_side_area_integration::profileRoundTrip` — feste Liste

Der Lauf erwartete die Seiten `{WinRotorLog, Rx}` wörtlich und bekam
`{WinRotorLog, Rx, Tx}`.

Ursache: beim Einschalten übernimmt der Seitenbereich die Applets, die
in der Spalte „versteckt" sind — und `MainWindow::appletHiddenInColumn`
zählt alles, was zu mehr als einem Viertel von anderen **Fenstern**
überdeckt ist. Was wen überdeckt, hängt am Schirm.

**Behoben:** der Lauf vergleicht gegen den eigenen Vorher-Stand
(`vorher = area->pageIds()`) statt gegen eine feste Liste — und prüft
damit, was er eigentlich wissen will: überlebt der Bereich den Weg
durchs Profil unverändert?

Gegenprobe: eine künstlich verfälschte Erwartung lässt ihn umfallen.

## Was offen bleibt

| Lauf | Befund |
| --- | --- |
| `tst_logbook_opens_at_its_size` | `aShrinkInTheFirstSecondStays`: erwartet 900×720 (`minimumSizeHint`), bekommt 900×700. Wieder Geometrie — Schriftgröße/Dichte |
| `tst_logbook_durchgang` | zwei Fälle, noch nicht untersucht |
| `tst_tci_remote_auth`, `tst_menus_of_native_fields`, `tst_command_bar` | fallen **nur im `-j6`-Lauf** um, einzeln grün — Parallel-Artefakte (Ports, Fensterfokus, Überdeckung) |

Ob die beiden Logbuch-Läufe vor dem Löschen des Sandkastens
(`~/.qttest/.../Longpath.settings`) schon umfielen, lässt sich nicht
mehr sagen — das Löschen geschah mitten in der Untersuchung. Sie hängen
erkennbar an gespeichertem Zustand.

## Die Regel daraus

Ein Prüflauf, der ein Bild greift, rechnet in **Bildpunkten** — und
`grab()` liefert auf jedem Retina-Gerät doppelt so viele davon wie das
Layout kennt. Ein Prüflauf, der Fenster zählt, darf keine feste Liste
erwarten. Und ein Prüflauf, der ein modales Menü öffnet, braucht einen
Ausweg, der nicht an Qt-Taktgebern hängt.
