# Umbenennung: NereusSDR → Longpath

**Beschlossen:** 20. August 2026, OE5SOS
**Ausführung:** in der Nacht ab 22:00 Uhr, **auf einem eigenen Zweig**
**Name:** `Longpath` — der lange Weg, den jeder DXer kennt

---

## Warum ein Fork und kein Neuanfang

NereusSDR gehört boydsoftprez (J.J. Boyd, KG4VCF). Große Teile sind
Ports aus **Thetis** (GPLv2-or-later, FlexRadio / Wigley / Samphire
u. a.), **AetherSDR** (GPLv3), **WDSP** (TAPR), **freedv-gui**,
**radae_nopy** (BSD-2), **r8brain** (MIT) und der **Anvelina-Gateware**
(GPLv3).

**Ansage des Betreibers, wörtlich:** „natürlich müssen und sollen alle
copyright deutlich sichtbar sein!!!"

Das ist auch die Rechtslage: die GPL erlaubt den Fork und das
Umbenennen, verlangt aber, dass jede Copyright-Zeile und jeder
Lizenzkopf erhalten bleibt und der Fork selbst unter GPL steht.

### Was dabei NICHT angefasst wird

* Kein `Copyright (C)`-Eintrag wird geändert, gekürzt oder verschoben.
* Kein Lizenzkopf in einer portierten Datei wird angefasst.
* `docs/attribution/THETIS-PROVENANCE.md`, `AETHERSDR-PORTS.md`,
  `FREEDV-GUI-PROVENANCE.md` bleiben unverändert und gültig.
* Die Autoren-Tags in portierten Zeilen (`//MW0LGE`, `//W2PA`,
  `//DH1KLM`, …) bleiben verbatim — die Prüfskripte laufen weiter.
* Die „Modification history (NereusSDR)"-Blöcke bleiben **stehen**.
  Sie beschreiben, was damals geschah; sie umzuschreiben wäre eine
  Fälschung der Geschichte. Neue Änderungen bekommen einen neuen Block
  „Modification history (Longpath)".

### Was dazukommt

In `README.md`, `About`-Dialog und `LICENSE`-Umfeld ein klarer,
unübersehbarer Absatz:

> Longpath ist ein Fork von **NereusSDR** (© J.J. Boyd, KG4VCF),
> das seinerseits ein Port von **Thetis** (© FlexRadio Systems,
> Doug Wigley, Richard Samphire MW0LGE u. a.) ist. Struktur und
> Teile des Codes stammen aus **AetherSDR** (© Jeremy, KK7GWY).
> DSP: **WDSP** (TAPR). Weitere Quellen siehe `docs/attribution/`.
> Lizenz: GPL.

---

## Der Umfang, gemessen

* **2 067 Dateien** enthalten die Zeichenkette `NereusSDR`.
* Dateien mit dem Namen im Dateinamen:
  `cmake/NereusBuildTag.cmake`, `hal-plugin/NereusSDRVAX.cpp`,
  `resources/icons/NereusSDR.{ico,rc,png,icns,iconset}`.
* `docs/attribution/source-artwork/NereusMeter*.jpg` — **bleiben**,
  das sind Quellbilder aus der Herkunftsdokumentation.

---

## Reihenfolge (nachts abzuarbeiten)

### 1. Zweig anlegen
`git switch -c rename/longpath`. **`main` bleibt unberührt**, damit am
Morgen ein Vergleich möglich ist und nichts verloren geht.

### 2. Die Einstellungen zuerst — sonst ist alles weg

`AppSettings::resolveConfigDir` baut den Pfad aus
`GenericConfigLocation + "/NereusSDR"`, die Datei heißt
`NereusSDR.settings` (`AppSettings.cpp:118` und `:127`). Auf macOS also
`~/Library/Preferences/NereusSDR/NereusSDR.settings`.

Nach dem Umbenennen sucht die App unter `…/Longpath/Longpath.settings`
— **die gesamte Anordnung wäre scheinbar verloren**. Deshalb:

* Neuer Pfad ist `…/Longpath/Longpath.settings`.
* Beim ersten Start: gibt es den neuen Pfad **nicht** und den alten
  **schon**, wird der alte Ordner **kopiert** (nicht verschoben).
  Kopieren, damit ein Rückweg bleibt und ein paralleler NereusSDR-Bau
  weiter läuft.
* Der XML-Wurzelknoten heißt `<NereusSDR>` (`AppSettings.cpp:405`,
  `:645`). Beim Lesen **beide** Namen akzeptieren, beim Schreiben
  `<Longpath>`.
* Test dafür, bevor irgendetwas anderes umbenannt wird.

### 3. Bau und Identität
* `project(Longpath VERSION 0.5.2 …)` in `CMakeLists.txt`
* `NereusSDRObjs` → `LongpathObjs` (auch in `tests/CMakeLists.txt`)
* `cmake/NereusBuildTag.cmake` → `cmake/LongpathBuildTag.cmake`,
  erzeugtes `NereusBuildTag.h` mit
* `app.setApplicationName("Longpath")` / `setOrganizationName("Longpath")`
  (`src/main.cpp:187`, `:189`)
* Bundle-Kennung `com.boydsoftprez.NereusSDR` → **`at.oe5sos.longpath`**
* `hal-plugin/NereusSDRVAX.cpp` → `LongpathVAX.cpp`; **Achtung**, der
  Name steht auch im POSIX-Shared-Memory-Schlüssel — Plugin und App
  müssen zusammenpassen, sonst schweigt VAX
* Symbole in `resources/icons/` umbenennen (Inhalt später neu zeichnen)

### 4. Sichtbares
* Fenstertitel, `About`-Dialog, `README.md`, `CONTRIBUTING.md`,
  `CHANGELOG.md`
* `/release`-Ablauf: DMG-, AppImage-, NSIS-Namen
* GitHub-Arbeitsabläufe in `.github/`

### 5. Was NICHT ersetzt werden darf
Ein blindes `sed -i 's/NereusSDR/Longpath/g'` würde die Geschichte
verfälschen. Ausgenommen bleiben:

* alle Zeilen in `docs/attribution/**`
* jede Zeile mit `Copyright`
* jeder Block „Modification history (NereusSDR)"
* jede Zeile mit `From Thetis`, `From AetherSDR`, `[v2.10.3` , `[@`
* Commit-Nachrichten und CHANGELOG-Einträge, die von damals berichten

### 6. Absichern
* Volles Gate (Bau + `ctest`), 702 Tests müssen grün bleiben
* `scripts/verify-thetis-headers.py`, `verify-freedv-headers.py`,
  `check-new-ports.py`, `verify-inline-tag-preservation.py`
* App starten, Einstellungen prüfen: **Anordnung muss da sein**
* Erst danach ein Vorschlag zum Zusammenführen — **nicht** selbst nach
  `main` schieben

---

## Offen für den Morgen danach

* Neues App-Symbol (das jetzige zeigt Nereus)
* GitHub: Organisation/Repo `Longpath` anlegen — muss der Betreiber tun
* Ob `main` umgestellt wird oder der Zweig erst begutachtet wird
