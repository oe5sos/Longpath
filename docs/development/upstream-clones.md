# Die Upstream-Klone

NereusSDR ist ein Port. Vier Fremdrepos sind darum keine Bequemlichkeit,
sondern Werkzeug: ohne sie können `verify-inline-tag-preservation.py`,
`verify-thetis-headers.py` und `check-new-ports.py` ihre Arbeit nicht
tun — und, das ist der Punkt dieser Seite, **sie sagen das nicht laut
genug.**

## Warum das eine eigene Seite wert ist

Bis 2026-08-18 fehlten drei der vier Klone auf der Arbeitsmaschine. Die
Folge war nicht „Prüfung fällt aus", sondern schlimmer:

* **286 Warnungen aus je einem Grund.** Wer 286 Zeilen sieht, liest
  keine. Der eine echte Befund darin fällt nicht auf.
* **Acht Zitate zeigten auf die falsche Gabelung.** Sie nannten
  `networkproto1.c:762..878`; bei ramdor hat die Datei 749 Zeilen, bei
  mi0bot 1273. Gemeint war mi0bot. Ohne den Klon war das eine Vermutung,
  mit ihm ein Nachweis — siehe `14f61793`.
* **Ein „grün", das nichts hieß.** Zitate auf einen fehlenden Klon
  wurden nicht geprüft, und „nicht geprüft" sieht im Ergebnis aus wie
  „geprüft und in Ordnung".

## Die vier

Alle als Geschwister des NereusSDR-Verzeichnisses. Die Prüfer suchen
genau diese Namen (`discover_sibling()` in
`scripts/verify-inline-tag-preservation.py`).

| Verzeichnis | Repo | Wofür |
| --- | --- | --- |
| `../Thetis` | `ramdor/Thetis` | Die Hauptquelle. Fast jedes Zitat. |
| `../mi0bot-Thetis` | `mi0bot/OpenHPSDR-Thetis` | HL2. Eine **Gabelung** von Thetis mit gleichnamigen, inhaltlich abweichenden Dateien. |
| `../deskhpsdr` | `dl1bz/deskhpsdr` | P1/P2-Protokolldetails. |
| `../freedv-gui` | `drowe67/freedv-gui` | RADE, FreeDV Reporter, PSK Reporter. |

```bash
cd ..   # neben das NereusSDR-Verzeichnis
git clone --filter=blob:none https://github.com/ramdor/Thetis
git clone --filter=blob:none https://github.com/mi0bot/OpenHPSDR-Thetis mi0bot-Thetis
git clone --filter=blob:none https://github.com/dl1bz/deskhpsdr
git clone --filter=blob:none https://github.com/drowe67/freedv-gui
```

**`--filter=blob:none`, nicht `--depth`.** Ein Partial Clone hat die
**volle Historie** — nötig, weil jedes Zitat seine Fassung nennt
(`[v2.10.3.13]`, `[@501e3f5]`) und der Prüfer sie über
`git show <rev>:<pfad>` aufschlägt. Ein flacher Klon kennt die alten
Commits nicht und meldet `stamp-not-in-clone`. Dateiinhalte holt git bei
Bedarf nach: mi0bot liegt so bei rund 21 MB statt einigen hundert.

## Keine zweite Arbeitskopie je Fassung

Ältere Anleitungen legten `../Thetis-v2.10.3.15` als eigenes
Verzeichnis an. Nötig ist das nicht mehr: der Prüfer liest die zitierte
Fassung direkt aus dem Objektspeicher (`ea77ea90`). Die
Verzeichnis-Pins in `THETIS_VERSION_DIRS` behalten Vorrang, damit CI sie
weiter per Umgebungsvariable hereinreichen kann.

## Nachsehen, ob es wirkt

```bash
python3 scripts/verify-inline-tag-preservation.py --audit
```

Die Kopfzeilen melden jeden fehlenden Klon einzeln. Die Warnungen
darunter sind nach Grund gruppiert; eine Gruppe `upstream-not-found` mit
dreistelliger Zahl heißt immer: hier fehlt eine Arbeitskopie, nicht hier
sind hundert Fehler.

```bash
python3 scripts/verify-inline-tag-preservation.py --unstamped
```

teilt die Zitate ohne Fassungsangabe in die, bei denen ein Stempel
mechanisch nachtragbar ist, und die, bei denen ein Mensch entscheiden
muss.
