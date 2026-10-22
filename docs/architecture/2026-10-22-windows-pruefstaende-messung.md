# Windows: bauen und laufen die Prüfstände dort? — gemessen am 2026-10-22

**Kurz:** ja. 1080 von 1082 Prüfstandsquellen übersetzen unter MinGW
unverändert, Linken trägt, und die Prüfstände laufen — sobald die DLLs
neben ihnen liegen. Eingeschaltet wird trotzdem nur eine benannte
Teilmenge, und der Grund ist der Preis, nicht die Portierbarkeit.

Dieses Blatt hält die Messwerte fest, weil die Kommentare in `ci.yml`
sie nur zusammengefasst tragen und weil die Kostenrechnung die Grundlage
für die eine offene Entscheidung ist (Prüfstände mit Oberfläche: ja oder
nein).

## Woran man geglaubt hat, und warum es falsch war

In `.github/workflows/ci.yml` stand seit dem 2026-08-09 (`c6b3eaa7f`):

> Windows stays single-job and test-free: its fakes really are unported
> (mingw paths), and that is a separate piece of work from this one.

Die Attrappen sind `tests/fakes/`: drei Dateien, 528 Zeilen —
`FakeAudioBus.h`, `P1FakeRadio.h`, `P1FakeRadio.cpp`. Darin steht **keine
einzige Präprozessor-Bedingung**, kein `Q_OS_*`, kein POSIX-Header, kein
Dateipfad. Ihre Einschlüsse sind `core/IAudioBus.h`, `QByteArray`,
`QString`, `QObject`, `QTimer`, `QUdpSocket`, `QHostAddress`,
`QNetworkDatagram`, `<algorithm>`, `<cstring>`, `<vector>`. Sie stammen
vom April 2026 und waren nie Linux-eigen.

```
$ grep -nE 'Q_OS_|#if(def)?|/tmp|/usr|/dev|unistd|sys/|mingw' tests/fakes/*
(kein Treffer)
```

Der Satz war also schon beim Schreiben falsch und hat 74 Tage gehalten.
Er ist deshalb bemerkenswert, nicht weil ihn jemand leichtfertig
geschrieben hätte — dasselbe Commit hat macOS vom testfreien Zustand
befreit und hat genau dabei erklärt, warum ein solcher Satz gefährlich
ist. Das Muster ist: *ein Kommentar, der eine Plattform für untestbar
erklärt, überlebt den Zustand, den er beschreibt.*

## Die drei Messläufe

Alle auf `windows-latest`, MinGW 13.1 (Qt-Werkzeug `tools_mingw1310`),
Qt 6.8.3 `win64_mingw`, Ninja, `RelWithDebInfo`.

### Lauf 39749806756 — was bricht überhaupt?

Konfiguriert mit `-DLONGPATH_BUILD_TESTS=ON`, sonst unverändert. Das
Konfigurieren lief auf Anhieb durch, kein Eingriff nötig:

```
Total Tests: 1082
```

Dann alle Objektziele unter `tests/` übersetzt, **ohne zu linken**, mit
`ninja -k 0` — damit ein Lauf jeden Fehler meldet statt nur den ersten:

```
Objektziele unter tests/: 2178
… 57 Minuten …
FAILED-Ziele insgesamt: 2
```

Zwei. Im Wortlaut:

```
tests/tst_radio_discovery_scan_bound.cpp:33:10: fatal error:
    arpa/inet.h: No such file or directory
tests/tst_sendefehler_deutung.cpp:72:47: error: 'EHOSTDOWN' was not
    declared in this scope; did you mean 'ENETDOWN'?
```

Linken, fünf Kernlogik-Prüfstände: `ninja`-Rückgabe **0**, keine
undefinierten Verweise, 105 s für fünf.

Darin steckt auch eine **widerlegte eigene Vermutung**: `CMakeLists.txt:1514`
hängt `ws2_32` an das App-Ziel `Longpath`, nicht an `LongpathObjs` —
Prüfstände linken nur gegen `LongpathObjs` und `Qt6::Test`. Ich habe
daraus vorhergesagt, dass das Linken an `setsockopt` scheitert. Es
scheitert nicht: Qt bringt `ws2_32` transitiv mit.

Was derselbe Lauf **nicht** sagen konnte: alle fünf gelinkten Prüfstände
meldeten `***Timeout 120.01 sec`. Nicht rot — hängend, alle fünf mit
derselben Zahl.

### Lauf 39762280534 — warum hängen sie?

Die zwei Übersetzungsfehler behoben, beide übersetzen (`ninja` 0,
0 Fehlerzeilen).

Zum Hänger: `objdump -p` auf `tst_amateur_bands.exe` nennt 21 DLLs. Jede
einzelne liegt irgendwo im Baum — aber nicht jede in einem Suchweg:

| DLL | liegt in | im PATH? |
| --- | --- | --- |
| `Qt6Core/Gui/Widgets/Network/Test/Svg/Multimedia/WebSockets` | `Qt/6.8.3/mingw_64/bin` | ja |
| `libstdc++-6`, `libgcc_s_seh-1`, `libwinpthread-1` | `Qt/6.8.3/mingw_64/bin` | ja |
| `ADVAPI32`, `AVRT`, `KERNEL32`, `SETUPAPI`, `USER32`, `WINMM`, `WS2_32`, `msvcrt`, `ole32` | `C:/Windows/System32` | ja |
| **`librade.dll`** | `build/` | **nein** |
| **`libfftw3-3.dll`** | `third_party/fftw3/bin/` | **nein** |
| **`libfftw3f-3.dll`** | `third_party/fftw3/bin/` | **nein** |

Dann derselbe Lauf dreimal, gleicher Bauordner, einziger Unterschied der
PATH:

```
a) wie es ist                 5 von 5  ***Timeout 120.01 sec
b) PATH + Qt/MinGW/FFTW/build 5 von 5  Passed,  0,66 s insgesamt
c) b) und QT_QPA_PLATFORM=offscreen
                              5 von 5  Passed,  0,31 s insgesamt
```

Es war also **nie die Prüflogik**. `librade.dll` wird per `POST_BUILD`
neben `Longpath.exe` gelegt (`CMakeLists.txt:1737`), die Prüfstände
liegen aber in `build/tests/`; die beiden FFTW-DLLs werden überhaupt nie
kopiert.

**Warum es wie ein Hänger aussieht und nicht wie ein Startfehler:** fehlt
eine DLL, zeigt Windows einen Fehlerdialog. Auf einem Läufer ohne
Bediener klickt den niemand weg, also steht der Vorgang bis zum
Zeitlimit. Wer das nicht weiß, sucht die Ursache im Prüfstand, wo sie
nicht ist.

Nebenbefund derselben Messung: `qoffscreen.dll` liegt in Qt 6.8.3 für
Windows bei, neben `qwindows.dll`, `qminimal.dll` und `qdirect2d.dll`.
Ein offscreen-Weg wäre auf Windows also vorhanden.

Und ein offener Rest: `tst_radio_discovery_scan_bound` übersetzt und
linkt nach dem Port, läuft aber auch mit richtigem PATH in die
Zeitüberschreitung (120 s). Das ist der eine Prüfstand mit einem echten
Verhaltensunterschied auf Windows, nicht mehr nur einem Pfadproblem —
siehe „Was offen bleibt".

### Lauf 39768259011 — die ganze Reihe, vier Teile

Die DLL-Behebung sitzt in `tests/CMakeLists.txt` (Ziel
`longpath_test_dlls`), nicht im Arbeitsablauf — dieser Lauf prüft sie
also mit. Ergebnis siehe die Tabelle am Ende dieses Blatts.

## Was die Behebung ist, und warum so

**`tests/tst_radio_discovery_scan_bound.cpp`** bekommt einen
Windows-Zweig: `<winsock2.h>` / `<ws2tcpip.h>` gegen die vier
POSIX-Dateien. Dabei fällt ein zweiter Fehler auf, den der Port sichtbar
macht: `SOCKET` ist unter Windows **vorzeichenlos**, die übliche Prüfung
`fd < 0` ist dort immer falsch und hätte einen gescheiterten `::socket()`
durchgelassen. Jetzt `INVALID_SOCKET`. Dazu ein `WSAStartup`, weil die
Flutfäden starten können, bevor Qt seine eigene Steckdose angelegt hat.

**`tests/tst_sendefehler_deutung.cpp`** liest jetzt die Tabellen des
Prüflings (`kKeinWeg`, `kVerweigert`, `kWarteschlange`, `kEigenerFehler`
aus `src/core/SendeFehler.h`) statt die POSIX-Namen nachzubauen. Das ist
die interessantere der beiden Stellen: `SendeFehler.h` löst dasselbe
Problem seit dem 2026-10-04 und sagt es im Kommentar auch —

> ein Name, den ein System nicht kennt (EHOSTDOWN fehlt unter MinGW —
> daran scheiterte der Windows-Bau am 2026-10-04), fällt hier auf und
> nicht erst im Lauf

— und der Prüfstand hat die Tabelle trotzdem verdoppelt. Auf Unix sind
die Tabellen des Prüflings elementweise dieselbe Liste, die vorher im
Prüfstand stand; es geht also keine Deckung verloren. Gewonnen: eine
neue Nummer in `SendeFehler.h` wird ab sofort mitgeprüft.

**Die DLLs** werden neben die Prüfstandsbinärdateien kopiert, statt den
PATH zu setzen. Grund: Windows durchsucht immer zuerst den Ordner der
ausführbaren Datei selbst — ohne PATH, ohne Umgebungsvariable, auch wenn
jemand einen Prüfstand von Hand startet. `ENVIRONMENT_MODIFICATION` auf
den ctest-Einträgen wäre die Alternative, braucht aber CMake 3.22; dieses
Projekt steht auf 3.20.

## Die Kostenrechnung — der eigentliche Unterschied zu Linux und macOS

Windows ist in dieser Datei absichtlich **ohne
Übersetzungszwischenspeicher** (kein sccache; die Begründung steht in
`ci.yml` bei „Windows intentionally skips sccache" — ein mehrstündiger
Ausfall des Azure-Blob-Backends während der Phase-3N-Erprobung, und
sccache hat kein sanftes Zurückfallen). Jeder Lauf übersetzt und linkt
kalt.

Gemessen:

| Posten | Messwert | Quelle |
| --- | --- | --- |
| App-Bau allein | 13 min 21 s | Lauf 39741382912, `main` |
| 2178 Objektziele unter `tests/` übersetzen | 57 min | 39749806756 |
| Linken je Prüfstandsbinärdatei | 21 s | 105 s / 5, 39762280534 |
| Fahren je appless-Prüfstand | 0,13 s | 0,66 s / 5, 39762280534 |

Daraus:

* **ganze Reihe, ein Läufer:** ~15 min App + 57 min Übersetzen + ~1,6 h
  Linken ≈ **2,5 h**, bei doppeltem Minutenpreis gegenüber Linux.
* **ganze Reihe, vier Teile:** ≈ 1 h je Teil, vier Läufer parallel —
  gleiche Läuferzeit, schnellere Antwort.
* **Teilmenge `appless` (189):** ~15 min App + ~10 min Übersetzen +
  ~17 min Linken ≈ **45 min** auf einem Läufer.

Zum Vergleich steht heute der Windows-Auftrag bei 13 min.

## Was eingeschaltet ist

Die Marke **`appless`** — jeder Prüfstand mit `QTEST_APPLESS_MAIN`, also
ohne `QApplication` und ohne Fenster. Hergeleitet aus der Datei selbst,
genau wie `core` / `models` / `gui`, nicht aus einer Tabelle, die jemand
pflegen muss. Damit entstehen von allein `ctest -L appless` und das
Bauziel `tests_appless`, auf allen drei Plattformen.

**189 von 1084** tragen sie (auf macOS gezählt). Lokal auf macOS gebaut
und gefahren: 189 von 189 grün in 236 s. 15 davon tragen auch die Marke
`gui` — sie binden `gui/`-Kopfdateien ein, bauen aber kein
`QApplication` auf.

Windows fährt `ctest --no-tests=error -L appless`. Das `error` statt des
bisherigen `ignore` ist Absicht: findet ctest unter der Marke nichts,
soll der Lauf **laut** abbrechen. Ein Tor, das nichts prüft und trotzdem
grün meldet, ist genau der Fehler, den dieselbe Datei am 2026-05-12
schon einmal beseitigt hat.

## Was offen bleibt

**Martins Entscheidung — Prüfstände mit Oberfläche auf Windows?** Dafür
braucht es einen Schirmweg (Linux nutzt `xvfb`, macOS das
offscreen-Plugin) und etwa die vierfache Läuferzeit. `qoffscreen.dll`
liegt bei, und die fünf appless-Prüfstände liefen damit sogar schneller
(0,31 s gegen 0,66 s) — aber an einem echten Fenster ist auf Windows
nichts davon geprüft. Eingebaut ist bis dahin die sparsame Annahme.

**`tst_radio_discovery_scan_bound` läuft auf Windows nicht durch.** Er
übersetzt und linkt nach dem Port, hängt aber auch mit richtigem PATH
120 s. Er ist nicht appless (`QTEST_MAIN`), fällt also nicht in die
eingeschaltete Teilmenge; wer ihn angeht, fängt bei der Frage an, ob der
Winsock-Flutfaden die Steckdose unter Windows überhaupt dauerhaft lesbar
macht — die ganze Prüflogik hängt daran.

**Die Zahl 451 in `ci.yml` ist überall veraltet.** Sie steht an acht
Stellen in Kommentaren und stammt vom Mai 2026; angemeldet sind 1077
über `longpath_add_test()` plus ein Dutzend direkt, insgesamt 1084 auf
macOS und 1082 auf Windows. Hier nicht mitkorrigiert — das sind
Zahlenstände in Begründungen zu Linux-Themen (Platten, Speicher,
Linkzeit) und gehören in einen eigenen Durchgang, nicht in diesen.

## Nicht angefasst

**Die ANAN-10-Tonverzerrung unter Windows** (CHANGELOG 0.6.3-rc1, seit
zwei Anläufen unverstanden). Sie ist der Grund, warum seit dem
2026-10-07 keine Windows-Pakete mehr ausgeliefert werden
(`release.yml`-Kopf). Sie braucht das echte Gerät und steht ausdrücklich
nicht in diesem Stand — dass die Prüfstände auf Windows laufen, sagt
über sie nichts.

**Die Auslieferung.** `release.yml` bleibt unverändert: hier wird nur die
Prüfung eingeschaltet, nicht die Paketherstellung wieder aufgenommen.
