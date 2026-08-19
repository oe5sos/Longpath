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

**Automatisch abgedeckt seit 2026-08-19** (`tst_tune_guide_behaviour`,
8 Fälle mit echten Mausereignissen im Offscreen-Malweg): dass die Linie
beim Bewegen **erscheint**, beim Verlassen des Panadapters **sofort**
verschwindet, dass eine zweite Bewegung sie am Leben hält, dass der
Schalter im Aus nichts tut und dass Wiedereinschalten auf die nächste
Bewegung wartet.

Die Zeilen bleiben trotzdem in der Matrix: der Test weiß, dass der
Zustand stimmt, aber nicht, ob man die Linie **sieht** — Farbe,
Lesbarkeit der Beschriftung, Flackern. Das kann nur ein Auge.

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

## §6 S-Verlauf: erkannte Signale als Marken — braucht Signal

Schalter: dieselbe Seite, „Mark detected signals (S-history)" und
„Mark suspected interference (QRM)". **Beide Vorgabe aus.**

Der Erkenner läuft nur bei eingeschaltetem Schalter und dann auf 10 Hz
gedrosselt. Zeile 6.1 ist deshalb die wichtigste der ganzen Matrix: sie
prüft, dass ein ausgeschaltetes Merkmal wirklich nichts kostet.

Die Rechenlast ist inzwischen am Schreibtisch gemessen (siehe 6.7) und
liegt bei einem Vierzigstel Prozent eines Kerns. Die Zeilen 6.1 und 6.7
bleiben trotzdem in der Matrix: gemessen wurde eine **Funktion**, nicht
die laufende Anwendung.

| # | Schritt | Erwartet | Ergebnis |
|---|---------|----------|----------|
| 6.1 | Beide Häkchen aus, CPU-Last im Bottom-Banner über eine Minute beobachten (verbunden, Wasserfall läuft) | Kein messbarer Unterschied zu vorher. Ausgeschaltet kostet das Merkmal einen Vergleich je Bild | |
| 6.2 | „Mark detected signals" ein, auf ein belegtes Band (z. B. 40 m abends) | Nach wenigen Sekunden erscheinen bernsteinfarbene Marken über den Stationen, beschriftet mit der S-Stufe (`S7`, `S9+10`) statt eines Rufzeichens | |
| 6.3 | Eine Station verstummt | Ihre Marke bleibt zunächst stehen und verschwindet nach etwa 30 s | |
| 6.4 | Auf ein leeres Band wechseln | Innerhalb einer Minute sind alle Marken weg (Eintrag verfällt nach 60 s) | |
| 6.5 | Marken und DX-Spots gleichzeitig einschalten, auf eine gespottete Station gehen | Wo ein Spot liegt, steht **nur** der Spot mit Rufzeichen — die S-Marke daneben wird unterdrückt (3-kHz-Regel). Keine zwei Etiketten übereinander. *Die Regel selbst ist seit 2026-08-19 automatisch geprüft (`tst_signal_history_markers`: 2 kHz unterdrückt, 4 kHz nicht, beide Richtungen, und der Speicher behält die Marke)* | |
| 6.6 | Viele Marken plus viele Spots gleichzeitig | Die Kollisionsstapelung gilt für beide Sorten zusammen; keine Überlappung, `+N`-Bündel zählen beide mit | |
| 6.7 | CPU-Last mit eingeschaltetem Merkmal beobachten | **Kein nennenswerter Anstieg erwartet.** Am Schreibtisch gemessen (`tst_signal_history_cost`, 2026-08-19, M-Mac): Erkenner **26 µs** je Bild auf einem Band mit 12 Stationen plus 2 Störern, Verwaltung **2,2 µs** je Durchlauf. Bei 10 Hz sind das **0,026 % eines Kerns**. Zeigt die Bank etwas anderes, liegt es nicht am Erkenner — dann notieren und melden | |
| 6.8 | „Mark suspected interference" zusätzlich ein, auf einen Dauerträger (z. B. Rundfunk-Splatter oder eine Trägerlinie) | Nach etwa 6 s wird der Träger **rot** markiert. Sprachsignale bleiben bernsteinfarben | |
| 6.9 | Ein langes QSO auf einer Frequenz beobachten | Es wird **nicht** rot: sprachbreite Signale brauchen zwei ununterbrochene Minuten ohne Lücke, und Sprache hat Lücken | |
| 6.10 | Beide Häkchen aus, während Marken stehen | Alle Marken verschwinden sofort | |
| 6.11 | Häkchen ein, App neu starten | Die Häkchen stehen noch auf ein, Marken bauen sich neu auf | |
| 6.12 | Bandplan mit Fonie-Bereichen aktiv, ein sprachbreites Signal außerhalb der Fonie-Bereiche (z. B. im CW-Teil) | Es bekommt **keine** Sprachmarke. Ein Schmalband-Störträger dort bekommt bei eingeschaltetem QRM-Häkchen sehr wohl eine | |

## §7 Doppelklick auf ein gespottetes Rufzeichen — braucht Spots

Voraussetzung: eine Spot-Quelle liefert (DX-Cluster, RBN, POTA …), damit
Etiketten im Panadapter stehen. Für die Zeilen mit Peilung zusätzlich der
**eigene Locator** unter Rotor-Einstellungen; QRZ-Zugangsdaten sind
nützlich, aber nicht nötig — die Richtung kommt notfalls aus `cty.dat`.

| # | Schritt | Erwartet | Ergebnis |
|---|---------|----------|----------|
| 7.1 | Doppelklick auf ein Spot-Etikett | Rotor/Log-Fläche kommt nach vorn, das Rufzeichen steht im Feld, Land und Flagge erscheinen | |
| 7.2 | Dasselbe, auf die Rose schauen | Der **Zeiger geht auf die Zielposition** des Rufzeichens; die Statuszeile nennt Entfernung, Grad und Himmelsrichtung. Der Mast bewegt sich **nicht** | |
| 7.3 | Ohne Netzverbindung wiederholen | Die Richtung erscheint trotzdem — sie kommt aus den Koordinaten der DXCC-Einheit in `cty.dat`, die Statuszeile sagt „(from prefix)" | |
| 7.4 | Mit QRZ-Zugang, kurz warten | Die Schätzung aus dem Prefix wird durch den genauen Locator ersetzt; Peilung und Entfernung springen auf den genauen Wert | |
| 7.5 | Der erste Klick des Doppelklicks | Das Gerät ist auf die Spot-Frequenz abgestimmt — der Log-Eintrag nimmt Frequenz, Band und Betriebsart von dort | |
| 7.6 | Danach „Log QSO" drücken | Der Eintrag enthält Rufzeichen, Frequenz, Band, Betriebsart, Locator, **Entfernung und Peilung** | |
| 7.7 | Doppelklick auf freie Fläche im Spektrum | Nichts wird ins Log übernommen; das bisherige Verhalten des zweiten Klicks bleibt | |
| 7.8 | Doppelklick auf eine **S-Verlauf-Marke** (`S7`) | Nichts passiert — eine S-Stufe ist kein Rufzeichen | |
| 7.9 | Rechtsklick → „Turn rotor to <call>" zum Vergleich | Dieser Weg **dreht** den Mast. Der Unterschied zum Doppelklick ist beabsichtigt | |
| 7.10 | Doppelklick auf ein Etikett, während der Rotor gerade dreht | Kein Durcheinander: der laufende Lauf wird nicht abgebrochen, nur das Ziel angezeigt | |

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
