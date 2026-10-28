# Die Prüfstände werden netzdicht

2026-10-28.

## Der Befund

`AppSettings::isSandbox()` ist genau eine Zeile:

```cpp
return !qEnvironmentVariable("LONGPATH_CONFIG_DIR").trimmed().isEmpty();
```

und `darfVonSelbstVerbinden()` ist `!isSandbox()`. An diesem einen
Schalter hängt alles, was von selbst nach außen greift: die acht
Spot-Dienste (über *einen* frühen Ausgang in
`RadioModel::restoreSpotClientAutoStartState`), der TCI-Dienst, der
rigctld-Server, die CAT-Kanäle samt Pseudoterminal, der Rotor, die
Spracherkennung und seit heute das Begleitgerät.

**Diese Variable setzte im Prüfbetrieb niemand.** Nicht
`tests/TestSandboxInit.cpp`, nicht `tools/run_tests.sh`. In jedem
gewöhnlichen `ctest`-Lauf war `darfVonSelbstVerbinden()` also **wahr**:
jedes Prüfprogramm durfte Verbindungen aufnehmen, Ports binden und
Dienste starten.

Der Schutz griff nur bei handgestarteten Sonden, wo jemand die Variable
selbst setzte — also genau dort, wo man ohnehin aufpasst, und nicht in
den 1100 Läufen, die unbeaufsichtigt durchgehen.

Das ist derselbe Vorfall wie am 2026-09-17, als sich eine Sonde mit dem
Rufzeichen des Betreibers anmeldete und damit **seine** eigene
Cluster-Sitzung aus dem Netz warf — nur eine Stufe tiefer. Damals
wurden die Dateien isoliert; das Netz wurde es nie.

## Wie es auffiel

Über eine Voraussetzungsprüfung in einem neuen Prüfstand
(`tst_begleitgeraet_regler::inDerSondeVerbindetNichtVonSelbst`):

```cpp
QVERIFY2(!AppSettings::darfVonSelbstVerbinden(),
         "Der Pruefstand laeuft nicht isoliert -- dann prueft dieser "
         "Fall nichts.");
```

Ohne diese Zeile wäre der Fall **grün** gewesen — weil die Voraussetzung
ausfiel. Er hätte nichts geprüft und das stillschweigend getan, solange
niemand hinsieht.

## Die Änderung

`TestSandboxInit` setzt die Variable jetzt mit, neben dem schon
vorhandenen `QStandardPaths::setTestModeEnabled(true)`.

**Ein eigener Ordner je Prozess**, kein fester Wert. Es müsste nur
irgendein Wert darin stehen, damit `isSandbox()` wahr wird — ein echter
Ordner löst aber noch etwas: der qttest-Sandkasten aus
`setTestModeEnabled` ist nach dem Kommentar in derselben Datei *„shared
across all test binaries in a run"*. `ctest` fährt mit `-j`, also
schreiben gleichzeitig laufende Prüfprogramme bisher in **dieselbe**
Einstellungsdatei. Ein Ordner je Prozess nimmt diese Kopplung heraus.

`QTemporaryDir` räumt beim Beenden auf, und die Abbaureihenfolge
stimmt: `AppSettings` ist ein funktionslokales `static`, wird also erst
beim ersten Zugriff angelegt — nach diesem globalen Objekt — und damit
**vor** ihm abgebaut. Der Ordner steht noch, wenn `AppSettings` seinen
letzten Zugriff tut.

**Wer die Variable selbst führt, gewinnt.** Steht sie schon, wird sie
nicht angetastet.

Scheitert das Anlegen des Ordners, bleibt es beim alten Verhalten. Ein
Prüfbetrieb, der daran hängenbleibt, wäre schlimmer als einer, der nur
die Dateien isoliert.

## Die Zusicherung ist festgenagelt

`tst_sonde_bleibt_offline::initTestCase()` verlangt, dass die Variable
beim Start gesetzt ist — **ohne** sie selbst zu setzen, denn dann prüfte
der Fall nur sich selbst. Gegenprobe gefahren: mit der alten Fassung von
`TestSandboxInit` fällt er um.

Dort ist außerdem `cleanup()` geändert: es stellt den Startwert
**zurück** statt ihn zu löschen. Vorher lief jeder Fall nach dem ersten
un-isoliert — genau das, was hier behoben wird.

## Fünf Prüfstände mussten nachziehen

Sie prüfen, was die Wache überspringt oder umleitet, und müssen sie
darum ausdrücklich abschalten. **Dass sie das vorher nicht mussten, ist
der Befund.**

Zwei davon habe ich vorhergesehen, zwei hat erst der Lauf gezeigt (von
1100 Prüfständen fielen genau diese zwei um), einer brauchte nur einen
Hinweis:

| Prüfstand | Warum | Risiko beim Abschalten |
|---|---|---|
| `tst_spot_auto_start` | prüft die Auto-Start-Entscheidung, die der frühe Ausgang überspringt | keins: alle Adressen unter `.example.org`, einer reservierten Zone ohne Dienst dahinter |
| `tst_start_watch` | prüft das Zählwerk, das `StartWatch` im Sandkasten mit Absicht überspringt | keins: schreibt nur eine Marke in eine Datei |
| `tst_app_settings_profile` | prüft die **Standard**-Pfadauflösung über `QStandardPaths` | keins: rechnet Pfade aus |
| `tst_settings_migration_longpath` | prüft die Übernahme von `NereusSDR/` nach `Longpath/` am Standardort | keins: schreibt nur in den Test-Sandkasten |
| `tst_credential_store_migration` | — | keins, war schon hinter `LONGPATH_KEYCHAIN_LIVE=1` eingezäunt |

Die letzten beiden Pfad-Fälle sind kein Widerspruch zur Änderung,
sondern ihre Kehrseite: `resolveConfigDir()` nimmt bei gesetzter
Variable genau diesen Ordner — das ist ihr Zweck. Es sind zwei Wege,
und für den *mit* Variable ist `tst_sandbox_data_dir` zuständig.

Der dritte brauchte nur einen Hinweis: ein Livelauf am Schlüsselbund
muss jetzt **beides** setzen, weil `CredentialStore` den Umzug im
Sandkasten mit Absicht überspringt (*„Nie aus einer Sandbox — sie würde
die echten alten Einträge löschen"*):

```sh
LONGPATH_KEYCHAIN_LIVE=1 LONGPATH_CONFIG_DIR= ./tst_credential_store_migration
```

Ohne das Zweite fällt der Fall um, ohne dass am `CredentialStore` etwas
kaputt ist.

## Was das nicht ist

Keine Wache, die alles abschaltet. Die Gegenprobe in
`tst_sonde_bleibt_offline` — ohne die Variable starten die Dienste wie
bisher — bleibt stehen und muss stehen bleiben. Ein Prüfbetrieb, der
die halbe Anwendung lahmlegt, prüft sie nicht mehr.

`tools/run_tests.sh` braucht nichts: die Variable wird je Prozess
gesetzt, nicht je Lauf.
