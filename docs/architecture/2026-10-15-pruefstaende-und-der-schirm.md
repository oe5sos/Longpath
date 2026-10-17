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

## Zweite Runde — die restlichen fünf

Nach der ersten Runde blieben fünf Läufe. Alle fünf gehören zur
selben Familie, alle fünf sind jetzt behoben.

### `tst_logbook_opens_at_its_size` — zu früh gemessen

`aShrinkInTheFirstSecondStays` nahm die Mindestgröße **10 ms nach
`show()`**. In 50-ms-Schritten gemessen:

```
  0 ms: minimumSizeHint 561x720   Fenster 1000x780
 50 ms: minimumSizeHint 561x617   Fenster 1000x780
150 ms: minimumSizeHint 561x617   Fenster 1000x700
```

Der frühe Wert (720, Werkzeugleiste noch umgebrochen) ging in
`smaller` ein — der Lauf verlangte am Ende eine **Vergrößerung** von
700 auf 720 und prüfte das Gegenteil dessen, was in seinem Namen
steht. Auf den CI-Läufern schwingt es schneller ein.

**Behoben:** erst auf das Einschwingen warten
(`QTRY_VERIFY_WITH_TIMEOUT(minimumSizeHint().height() <= 650, 1000)`),
dann `smaller` bilden. Und die Vorbedingung mit **UND** statt ODER:
ist eine Kante nicht wirklich kleiner, bezeugt der Lauf sie auch
nicht.

### `tst_logbook_durchgang` — unstet, 2 von 3

`editingChangesOnlyThatField` und `editingAFieldStillTidiesIt` fielen
in zwei von drei Läufen um: es wurde schlicht nichts bearbeitet. Der
Auto-Antworter für modale Dialoge hing an einem 50-ms-Taktgeber — in
der Cocoa-Schleife eines modalen `exec()` kommt der mal an und mal
nicht.

**Behoben:** `ModalAnswerer`, ein Ereignisfilter auf `QEvent::Show`,
der die Antwort postet. Viermal hintereinander grün (vorher 2 von 3
rot).

### Drei Läufe, die nur parallel umfielen

`tst_tci_remote_auth`, `tst_menus_of_native_fields`, `tst_command_bar`
— einzeln grün, im `-j6`-Durchgang rot.

Zwei davon trugen je eine **eigene Abschrift** desselben brüchigen
Menü-Helfers und lasen `parentSeen` sofort nach dem Klick: unter Last
stand da noch der Wert von vorher („Compared QObject pointers are not
the same"). Die Technik liegt jetzt einmal in `tests/MenueFaenger.h`,
und alle drei Menü-Läufe benutzen sie — auch der aus der ersten Runde,
damit es nur eine Fassung gibt.

Beim dritten wartete `loopback_bekommt_den_init_burst_ohne_anmeldung`
2000 ms auf den Init-Burst (rund 98 Rahmen). Sechs gleichzeitig
gestartete Läufe holten den Fehlschlag reproduzierbar zurück (1 von
6); mit 15 000 ms sind es 0 von 6. Großzügig warten kostet nichts —
die Zeit läuft nur im Fehlerfall ab.

Gegenprobe unter Last: 16 von 16 parallelen Läufen grün.

## Die Regel daraus

Ein Prüflauf, der ein Bild greift, rechnet in **Bildpunkten** — und
`grab()` liefert auf jedem Retina-Gerät doppelt so viele davon wie das
Layout kennt. Ein Prüflauf, der Fenster zählt, darf keine feste Liste
erwarten. Und ein Prüflauf, der ein modales Menü öffnet, braucht einen
Ausweg, der nicht an Qt-Taktgebern hängt.
