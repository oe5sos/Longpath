# Panadapter-Merkmale — Bench Verification Matrix (2026-08-19)

Umfang: die vier Bedienelemente, die am 2026-08-19 aus der
Merkmalsliste gelandet sind, plus die Squelch-Linie vom Vortag, die noch
keine Bank gesehen hat.

* `bd287daf` Squelch-Linie
* `92c2acf5` Abstimmhilfe
* `6b183799` Squelch-Automatik
* `dc103721` Verlängerung in den Wasserfall abschaltbar

Liste und Begründungen:
[docs/design/2026-08-19-panadapter-merkmalsliste.md](../../design/2026-08-19-panadapter-merkmalsliste.md)

Automatische Abdeckung ist grün (23 Fälle über vier Testdateien,
`tst_squelch_line`, `tst_tune_guide`, `tst_auto_squelch`,
`tst_extended_overlay`; volles Tor 680/680). Die Testfälle prüfen den
**Zustand** — das gemalte Bild prüft nur diese Matrix.

## Bank

Die Zeilen in §1 bis §3 brauchen **kein Funkgerät**: sie hängen an Maus,
Menü und Persistenz. Die Zeilen in §4 brauchen ein **verbundenes Gerät
mit Signal** (ANAN-G2 oder HL2), weil sie einen gemessenen Rauschboden
voraussetzen. §5 braucht Senden.

Ergebnisspalte füllen mit PASS / FAIL / DONE_WITH_CONCERNS plus einer
Zeile Notiz.

## §1 Abstimmhilfe (`92c2acf5`)

Schalter: Setup → Display → Spectrum Defaults → Spectrum Overlays →
„Show tune guide".

| # | Schritt | Erwartet | Ergebnis |
|---|---------|----------|----------|
| 1.1 | Frischer Start, Häkchen anschauen | Aus. Im Panadapter keine Linie am Zeiger | |
| 1.2 | Häkchen setzen, Maus im Spektrum bewegen | Senkrechte Linie am Zeiger über die Spektrumshöhe, Beschriftung mit der Frequenz auf das Hertz in Punktgruppen (`14.270.000`) | |
| 1.3 | Maus stillhalten, vier Sekunden warten | Linie und Beschriftung blenden aus. Erneute Bewegung bringt sie zurück | |
| 1.4 | Zeiger an den rechten Rand des Spektrums führen | Beschriftung springt nach links vom Zeiger, statt aus dem Bild zu laufen | |
| 1.5 | Zeiger an den oberen Rand führen | Beschriftung springt unter den Zeiger | |
| 1.6 | Zeiger über eine Kerbe (TNF) führen | Abstimmhilfe verschwindet, der Kerben-Hinweis bleibt allein. Kein Flackern beim Ein- und Ausfahren | |
| 1.7 | Zeiger aus dem Panadapter heraus bewegen | Linie verschwindet sofort, nicht erst nach vier Sekunden | |
| 1.8 | App beenden und neu starten | Häkchen steht noch auf ein, Verhalten wie 1.2 | |
| 1.9 | Häkchen aus, während die Linie sichtbar ist | Linie verschwindet auf der Stelle | |

## §2 Verlängerung in den Wasserfall (`dc103721`)

Schalter: dieselbe Seite, „Extend VFO line into waterfall" und
„Extend passband shading into waterfall".

| # | Schritt | Erwartet | Ergebnis |
|---|---------|----------|----------|
| 2.1 | Frischer Start, beide Häkchen anschauen | **Beide ein.** Das ist der Istzustand von vorher; eine Vorgabe darf das Bild nicht umstellen | |
| 2.2 | „Extend VFO line" aus | Mittellinie und die beiden Filterkanten enden am unteren Spektrumsrand; im Wasserfall keine senkrechten Linien mehr. Die Durchlassfläche im Wasserfall bleibt | |
| 2.3 | „Extend passband shading" aus (Linie wieder ein) | Durchlassfläche endet am Spektrum, Linien laufen weiter durch. Die beiden Schalter wirken unabhängig | |
| 2.4 | Beide aus, Bandplan-Streifen eingeschaltet | Die Linien enden über dem Bandplan-Streifen, nicht darin | |
| 2.5 | Beide aus, neu starten | Beide bleiben aus | |

## §3 Squelch-Linie (`bd287daf`, Nachtrag vom Vortag)

| # | Schritt | Erwartet | Ergebnis |
|---|---------|----------|----------|
| 3.1 | Squelch einschalten (RxApplet / VFO-Flagge) | Bernsteinfarbener waagerechter Strich auf Schwellenhöhe, Beschriftung `SQL -92 dBm` links darüber | |
| 3.2 | Drei Sekunden warten | Strich blendet aus. Der Squelch bleibt eingeschaltet — die Linie ist eine Anzeige, nicht die Schwelle | |
| 3.3 | Schwelle verstellen | Strich erscheint erneut und sitzt auf dem neuen Wert | |
| 3.4 | Schwelle unter den unteren Rand des dBm-Bereichs stellen | Kein Strich am Rand „geklebt", er wird weggelassen | |

## §4 Squelch-Automatik (`6b183799`) — braucht Signal

Schalter: dieselbe Seite, „Auto squelch follows noise floor" plus
„Margin". **Voraussetzung: Squelch für die Scheibe eingeschaltet**, sonst
schreibt die Automatik absichtlich nichts.

| # | Schritt | Erwartet | Ergebnis |
|---|---------|----------|----------|
| 4.1 | Häkchen anschauen, frischer Start | Aus; die Margin-Spinbox ist gesperrt, solange es aus ist | |
| 4.2 | Automatik ein, Squelch **aus** | Die Schwelle im RxApplet bewegt sich **nicht**. Der gemerkte Wert des Betreibers bleibt unangetastet | |
| 4.3 | Squelch ein, Automatik ein, Margin 10 dB | Schwelle stellt sich auf Rauschboden + 10 dB. Zur Kontrolle „Show noise floor" einschalten: die Squelch-Linie liegt 10 dB über der NF-Linie | |
| 4.4 | Margin auf 5, dann auf 20 stellen | Schwelle folgt sofort, ohne dass sich der Rauschboden ändern muss. Abstand zur NF-Linie stimmt in beiden Stellungen | |
| 4.5 | Band mit anderem Rauschboden wählen (z. B. 40 m gegen 10 m) | Schwelle wandert innerhalb weniger Sekunden mit, Abstand bleibt | |
| 4.6 | Bei ruhigem Band eine Minute zuschauen | Kein Zappeln: die Schwelle rührt sich nur bei mindestens 1 dB Bewegung. Kein sichtbares Dauerflackern der Squelch-Linie | |
| 4.7 | Automatik aus | Die letzte gestellte Schwelle bleibt stehen (die Automatik nimmt nichts zurück) | |
| 4.8 | Automatik ein, neu starten, Squelch ein | Häkchen und Margin sind zurück, die Schwelle stellt sich binnen ein bis zwei Sekunden neu ein | |

## §5 Squelch-Automatik unter Senden — braucht Senden

| # | Schritt | Erwartet | Ergebnis |
|---|---------|----------|----------|
| 5.1 | Automatik + Squelch ein, dann MOX oder TUNE mit Dummy-Load | Während des Sendens rührt sich die Schwelle **nicht**, obwohl der gemessene Boden hochgeht | |
| 5.2 | MOX loslassen | Kein zugezogener Squelch danach: die Schwelle steht noch dort, wo sie vor dem Senden stand, und stellt sich normal weiter ein | |

---

## Bekannte Auslassungen

* **Mehrere Panadapter.** Die drei neuen Schalter geben ihren Zustand
  nicht an Geschwister-Widgets weiter (AetherSDR tut das über
  `window()`). Bis Phase 3F gibt es genau ein Panadapter; die Zeile ist
  dann neu zu prüfen.
* **Der Modelldatei-Erkenner** aus dem S-Verlauf ist nicht gebaut und
  steht als Vorschlag beim Betreiber — siehe Merkmalsliste, vierte
  Korrektur.
* **Pixelvergleiche** gibt es absichtlich nicht: sie hängen an
  Bezugspegel, Spanne und Kantenglättung und brächen beim nächsten
  Feinschliff, ohne dass eine Regel verletzt wäre.

## Modification history (NereusSDR)

* 2026-08-19 — angelegt von Martin Fischer, KI-gestützt über Anthropic
  Claude (Cowork).
