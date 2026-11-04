# Das Contestprogramm als Fenster in Longpath

2026-11-04. Betreiber am 2026-11-03: „vielleicht sollte man das alles in
longpath automatisch als option zusätzlich einbauen, sodass man nicht
primär beide programme offen haben muss. sprich das könnte in longpath
als eigenes contestprogramm auch laufen, es soll aber trotzdem alles auch
einzeln funktionieren."

## Die Entscheidung: ein Prozess, zwei Programme, dasselbe Log

Nicht gebaut wurde ein zweites Contest-Log in Longpath. Das
Contestprogramm (oe5sos/Contestprogramm, eigenes Repo, GPLv3, Qt6/C++20
wie Longpath) ist bereits eine Bibliothek `ContestprogrammCore` mit einem
dünnen `main.cpp`. Longpath holt es als **Submodul**
`third_party/contestprogramm`, linkt die Bibliothek und tut im eigenen
Prozess, was dort `main.cpp` tut: Datenverzeichnis, Einzelplatz-Wache,
Controller, Datenbank, Hauptfenster, Stylesheet.

| | eigenständig | als Fenster in Longpath |
| --- | --- | --- |
| Log | `<GenericData>/Contestprogramm/Contestprogramm/contestprogramm.sqlite` | **dasselbe** |
| CAT | rigctld (TS-590 über Hamlib, oder Longpath auf 4532) | Longpaths eigener `RigctldServer` auf 4532, nur lesend |
| Wasserfall | TCI 50001 von Longpath | **dasselbe** über localhost |
| Oberfläche | eigenes App-Stylesheet | dasselbe Stylesheet, aber am Fenster statt an der Anwendung |
| Zwei zugleich? | — | die Einzelplatz-Wache weist den zweiten Schreiber ab |

Beides bleibt für sich lauffähig: das Contestprogramm ändert für die
Einbettung nur seine CMake-Wachen (eigenständig = `CMakeLists.txt` ist
das Wurzelprojekt) und den Weg zum Datenverzeichnis.

## Was wo liegt

**Contestprogramm** (PR oe5sos/Contestprogramm#70, `feat/einbettbar`):

* `CMakeLists.txt`: `CONTESTPROGRAMM_EIGENSTAENDIG` — nur das Wurzelprojekt
  baut die ausführbare Datei, kopiert Ressourcen und registriert
  Prüfstände; eingebettet entsteht nur `ContestprogrammCore`.
  Include-Pfade mit `BEFORE`, die Verzeichnis-Includes des Wirts werden
  gelöscht — beide Bäume haben ein `src/core` mit gleichnamigen Köpfen
  (`Maidenhead.h`, `RigctldClient.h`, `DxClusterClient.h`, …).
* `src/app/Datenverzeichnis.{h,cpp}`: `standardDatenverzeichnis()` aus
  `GenericDataLocation`, nicht `AppDataLocation` — die hängt am
  Prozessnamen, und der heißt eingebettet „Longpath". `main.cpp` geht
  durch dieselbe Funktion.
* `tests/test_einbettung.cpp`: Pfad ohne Prozessnamen, Umgebung geht vor,
  Fenster und Controller entstehen im fremden Prozess ohne App-Stylesheet.

**Longpath** (dieser Zweig):

* `third_party/contestprogramm` — Submodul, gepinnt.
* `CMakeLists.txt`: Option `LONGPATH_CONTESTPROGRAMM`, Vorgabe AN wenn das
  Submodul ausgecheckt ist, sonst AUS (die CI checkt ohne Submodule aus
  und baut wie bisher). `add_subdirectory(... EXCLUDE_FROM_ALL)`, dazu die
  Brücke `LongpathContestBruecke` als eigene kleine Bibliothek: **nur
  `ContestFenster.cpp` sieht die Include-Pfade des Contestprogramms**,
  `ContestFenster.h` zieht nichts davon herein. `LongpathObjs` bekommt
  `LONGPATH_HAVE_CONTESTPROGRAMM` und die Brücke als PUBLIC-Abhängigkeit,
  damit Longpath und jeder Prüfstand, der `LongpathObjs` linkt, die
  Symbole haben.
* `src/contest/ContestFenster.{h,cpp}`: `oeffne()` / `schliesse()` /
  `istOffen()`; `restartRequested` des Contestprogramms (Farbthema,
  Wiederherstellung) wird zu „Fenster zu und wieder auf".
* `src/gui/MainWindow.cpp`: Tools › „Contest Logbook (Contestprogramm)…"
  und der Schalter „Open contest logbook at launch"
  (`ContestprogrammBeimStart`). Beim Öffnen wird Longpaths rigctld-Server
  für die Sitzung gestartet, falls er nicht läuft — **nur lesend**, PTT
  bleibt beim Schalter `RigctldAllowPtt` (Vorgabe aus; Betreiber
  2026-11-01: „Nein, nur lesen"). Die Einstellung `RigctldEnabled` bleibt
  unangetastet.
* `tests/tst_contestfenster.cpp`: eigener Datenordner, auf/zu/wieder auf,
  ein zweiter Schreiber wird abgewiesen.
* `werkzeug/longpath-auto-install.sh` (außerhalb des Repos): zieht das
  Submodul nach dem Checkout nach, damit die installierte Fassung das
  Fenster hat.

## Was bewusst NICHT passiert

* Kein in-Prozess-Kurzschluss zwischen RadioModel und Log. CAT und
  Wasserfall gehen über localhost wie bei zwei Programmen; das hält die
  Trennung, die der Betreiber verlangt („trotzdem alles auch einzeln").
  Ein direkter Weg (Spots in beide Richtungen, Stufe 1 der Leiter vom
  2026-11-01) bleibt offen und würde beiden Betriebsarten gleichermaßen
  zugutekommen, wenn er über TCI läuft.
* Der SRTM-Kachelspeicher des Contestprogramms (`SrtmTileLoader`) hängt
  weiter an `AppDataLocation` und landet eingebettet in Longpaths Ordner.
  Ein Zwischenspeicher, kein Log; später auf `Datenverzeichnis` umziehen.
* Die Sprache: Longpaths Menü ist englisch (`tst_oberflaeche_englisch`),
  das Contestprogramm deutsch. Das Fenster bleibt deutsch — es ist das
  Contestprogramm.

## Belege

Siehe PR: Prüfstände beider Seiten, Bau mit und ohne Submodul, und das
Fenster am laufenden Longpath.
