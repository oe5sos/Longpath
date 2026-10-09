# Sendeton-Optimierer — vier Profile aus einem Durchgang

**Stand 2026-10-09.** Gebaut auf `feat/sendeton-optimierer`, Arbeitsbaum
`~/Longpath/worktrees/optimierer`. Läuft **neben** dem bisherigen
Stimmcheck, nicht an seiner Stelle.

---

![Der Optimierer, 2,7-kHz-Profil](../images/optimierer-2700.png)

*Oben das 2,7-k-Profil. [Das schmale 2,3 k](../images/optimierer-2300.png)
zeigt in „FORGETS" Roehre, Exciter und Nachhall;
[4,0 k mit einem Mikrofon, das bei 2,8 kHz schon unten ist](../images/optimierer-4000-warnung.png),
zeigt die Warnzeile. Alle drei aus `tst_broadcast_optimizer_panel`
gezeichnet — ohne Funkgeraet fuellt sich das Panel nicht.*

---

## Wie es ausging

Der Auftrag war ein Optimierer: fünfzehn Sekunden sprechen, daraus vier
fertige Profile. Einen Tag lang wurde das gegen Martins Stimme gefahren.
Fünf echte Fehler kamen dabei heraus und sind behoben — sie stehen
unten, weil jeder von ihnen etwas über diese Art Arbeit sagt. Der
Befund darunter blieb bestehen:

> „Das Original-Audio ist gut, nur sämtliche Bearbeitungen sind eine
> Katastrophe." · „also besser wird das audio nicht." ·
> **„ich glaube, wir lassen das automatische verbessern."**

Daraufhin umgebaut, auf Ansage und nach AetherSDRs Vorbild
(`~/Longpath/AetherSDR/src/gui/AetherialAudioStrip.cpp`):

> „bei aether konnte ich es allein sehr gut machen … schaue bitte, wie
> aether es gemacht hat. so werden wir es auch machen."

**Was der Unterschied war.** Aether zeigt alle Stufen gleichzeitig in
einem zweispaltigen Raster. Longpath zeigte eine Stufe je Reiter. Wer
von Hand einstellt, muss die ganze Kette sehen, während er an einer
Stufe dreht — sonst stellt er acht Dinge nacheinander ein und hört erst
am Ende, dass sie sich widersprechen.

Nebenbei richtiggestellt: **Aether kann Profile speichern.** Martin
erinnerte es anders; `ChannelStripPresets` hat Speichern, Laden,
Löschen, Export einzeln, Export der Bibliothek und Import.

---

## Was es jetzt ist

**Ein Übersichtsreiter „Strip"** vor den acht Stufenseiten, mit einer
Karte je Stufe: Name versal, Zustand als Kapsel, die drei Zahlen dieser
Stufe monospaced, ein Knopf in die volle Seite. Leserichtung = Ketten­
reihenfolge (Gate, EQ, De-Esser, Kompressor, Röhre, PUDU, Nachhall,
Begrenzer).

> Erster Versuch, verworfen: Longpaths Stufenseiten direkt ins Raster.
> Aethers Panels sind kompakt, Longpaths sind für eine ganze Reiterseite
> gebaut — allein das Gate bringt ein 280 px hohes Kennlinienfeld mit.
> Zwei Spalten davon erzeugten waagrechtes Scrollen und zeigten
> trotzdem eine Stufe. Im gezeichneten Bild gesehen, nicht am Gerät.

**Ein Messblatt** im Reiter „Voice check" — was die Aufnahme ergab,
nicht was daraus gemacht wird: Grundton, LUFS/LRA, Störabstand,
Zischfrequenz samt Pegel, der Punkt an dem das Mikrofon aufhört, und
der gewählte Kanal. Dazu je Breite ein paar Sätze, was das für sie
heißt.

**Vier Kanäle**, mit Martins Zahlen: Contest 200–2500, dann 150–2700,
100–3200, breit 50–4050. Unten liegt der größere Teil des Unterschieds
— 50 gegen 200 Hz sind zwei Oktaven Körper.

**Zwei Knöpfe zum Hören:** *Raw* spielt den Durchgang nur durch den
Kanal, *Through your strip* durch die eigene Einstellung und dann den
Kanal. Das A/B vergleicht also seine Arbeit mit dem Original, nicht
eine Herleitung mit seiner Arbeit.

**Ein Knopf zum Ablegen:** die Kette, wie sie gerade steht, unter dem
Namen dieser Breite. Das ist die ganze verbliebene Automatik, und sie
erfindet nichts.

---

## Die fünf Fehler, und was sie gemeinsam hatten

Alle fünf waren dasselbe: eine Entscheidung, für die die Messung nicht
trug.

1. **Eine Contest-Zielkurve für alle vier Breiten.**
   `VoiceAnalyzer::targetDb` verlangt 125 Hz bei −12 dB gegen 1 kHz.
2. **Überhaupt ein absolutes Ziel.** Das Spektrum einer Stimme fällt
   natürlicherweise; darauf zu zielen bügelt sie flach. Gemessen:
   8,3 dB Körper weg beim *breiten* Profil.
3. **Gestapelte Schnitte.** Hochpass beim 1,8-fachen des Grundtons
   (bei 110 Hz also 198 Hz, mitten in der Stimme) plus Tiefenfilter
   knapp darüber.
4. **Brummkerben auf dem Grundton.** Der Detektor summiert ±23 Hz um
   50/100/150/200 Hz; eine 110-Hz-Stimme legt Grundton und zweite
   Oberwelle hinein. Ohne jeden Brumm gemessen: 110 Hz −12,5 dB,
   150 Hz −17,6 dB, 220 Hz −3,7 dB. **Der Detektor ist unverändert —
   `StripTuner` hängt an derselben Funktion.**
5. **Stufen, die ungefragt mitliefen.** De-Esser klammerte auf
   mindestens 3 dB, lief also immer; Rauschsperre lief in jedem leisen
   Raum.

Dazu zwei Fehler im Hören selbst, die den Vergleich wertlos machten:
das Kanalfilter wurde beim Abhören gar nicht angewandt (alle vier
klangen gleich), und als es angewandt wurde, war es zweimal dieselbe
zweite Ordnung statt eines Butterworth vierter — −6 dB schon auf der
Ecke, Abfall eine Oktave darunter.

---

## Was aus der Zeus-Durchsicht blieb

Am Gerät abgelesen (siehe Gedächtnis-Eintrag). Übernommen und
geblieben: die **gemessene** Zischfrequenz statt einer festen, LUFS und
LRA als Maß, und die Erkenntnis, dass Zeus' drei Stile **keinen
Klangregler** anfassen. Nicht geblieben: alles, was daraus eine
Automatik machen sollte.

---

## Historie: der ursprüngliche Auftrag

Martin, 2026-10-09, in drei Nachrichten:

> „stimme analysieren, verbessern, optimieren, möglichkeiten aufzeigen und
> daraus resultierend ein 2,3 – 2,7 – 3,2 und 4 k Profil zu erstellen"

> „ziel ist es, vielleicht 15 sekunden zu sprechen ohne einstellungen als
> basis und daraus sollte dann sehr sorgfältig alles genauest automatisch
> analysiert werden, um ein bestmögliches profil zu bekommen. dieses kann
> man danach noch, sofern gewünscht abändern. daraus sollte dann auch
> gleich automatisch die anderen profile entstehen"

> „man könnte das parallel zu unserem bisherigen audio setup auch gestalten
> und später entscheiden wir oder wir verwenden das bisherige."

Dazu zwei Nachträge, die den Entwurf an zwei Stellen geändert haben:

> „zeus ist hier sicherlich mit allen seinen tools der maßstab"

> „zb bei contest muss auch diverse filter dann vergessen werden, die würde
> man dann eher ab 2,7 profil nutzen"

---

## Was schon dastand, und was wirklich fehlte

Der Baum hatte den größten Teil bereits:

| Vorhanden | Was es tut |
| --- | --- |
| `VoiceAnalyzer` | misst zehn Bänder, Rauschteppich, Scheitelfaktor, Brumm, Zischen, Übersteuerung |
| `StripTuner` | setzt daraus **eine** Kette |
| `StripTargets` | Zielkurven mit `topHz` |
| `StripSettings` | benannte Profile speichern/laden |
| `TxVoiceCheckDialog` | nimmt 15 s auf (roh **und** hinter dem Strip), spielt beide ab |

Es fehlte genau das Stück dazwischen: aus **einer** Messung **vier**
bandbreitenabhängige Profile ableiten, sagen, was jedes kostet, und jedes
einzeln hörbar machen.

---

## Zeus als Maßstab

Nachgesehen in `~/Longpath/zeus-station-engine` (v2.0.26). Zwei Befunde,
beide übernommen:

**1. Ein Profil trägt den Kanal mit.** `TxAudioProfileDto`
(`Zeus.Contracts/Dtos.cs:2944`) ist „a single operator-named macro that
captures the ENTIRE TX-audio shaping state" — und führt `LowCutHz` /
`HighCutHz` neben der Kette. Ein „2,3 k Profil", dessen Sendefilter noch
auf 2,7 kHz steht, ist keines. `Plan` trägt beide Werte deshalb mit.

**2. Eine Dichte-Achse statt Verhältnis und Schwelle.**
`TargetSpectralDensity`, ganzzahlig 0..100, Vorgabe **55**
(`Station.Engine.Hosting/TxFidelityPolicyStore.cs:DefaultTargetn`). Das
ist die bessere Bedienung und ist unverändert übernommen: der Bediener
bewegt **eine** Zahl, der Kompressor wird daraus *gerechnet*.

Die Benennung der Stufen (Natural / Broadcast / Dense / Maximum) steht im
Zeus-Client, nicht im Maschinen-Quelltext. Die Zahlen dahinter sind
Longpaths eigene Setzung auf Zeus' Skala — Broadcast liegt auf Zeus'
dokumentierter Vorgabe 55, der Rest verteilt sich darum.

---

## Über den Tellerrand: was die Musikbranche hergibt

Gesucht nach frei verwendbaren Bausteinen. Drei Befunde, einer davon
eingebaut, zwei als Vorschlag offen.

### Eingebaut: LUFS und LRA (BS.1770 / EBU Tech 3342)

`src/core/Loudness.{h,cpp}`, Longpath-eigen, kein fremder Quelltext.

Der Analysator hatte drei hausgemachte Pegelzahlen, von denen zwei
dasselbe maßen und keine außerhalb des Programms etwas bedeutete. Der
Hausstil sagt dazu das Nötige: *zwei Zahlen für eine Rolle sind schlimmer
als die falschere von beiden.* Also:

* **LUFS** — K-gewichtete Lautheit mit dem zweistufigen Tor.
* **LRA** — Lautheitsumfang, der Normmaßstab für „wandert der Pegel".

Der Gewinn ist nicht die Norm an sich, sondern dass **vorher und nachher
vergleichbar** werden: denselben Durchgang durch ein Profil rechnen, neu
messen, und die LUFS-Differenz *ist* der gewonnene Durchschnittspegel,
während die LRA-Differenz zeigt, was es an Leben gekostet hat. Ein
Scheitelfaktor kann das nicht, und das Ohr kann es nicht zweimal
zuverlässig.

Die K-Gewichtung wird aus den zeitkontinuierlichen Entwurfsgrößen per
Bilineartransformation **gerechnet**, nicht als 48-kHz-Tabelle
abgeschrieben — so arbeitet der Analysator bei jeder Abtastrate. Der
Prüfstand stellt die Ableitung gegen die veröffentlichte 48-kHz-Tabelle:
träfe sie die fünf Zahlen nicht, wäre entweder die Transformation oder
eine der vier Entwurfskonstanten falsch.

> Die Entwurfsgrößen stammen aus Zweitquellen (veröffentlichte Tabelle,
> Umsetzungen, die sie herleiten); das ITU-Papier ist kostenpflichtig.
> Der Tabellenabgleich ist die stärkste Prüfung, die ohne das Papier
> möglich ist.

### Vorschlag, noch nicht eingebaut: Airwindows `ClipOnly2`

MIT, Chris Johnson. Ein Begrenzer kappt, ein **Clipper** schert ab — und
in einer Rundfunkkette steht der Clipper ganz am Ende. `ClipOnly2`
unterdrückt dabei die Helligkeit des digitalen Clippens, ohne
unbeschnittene Abtastwerte anzufassen; der Zustand dafür ist winzig (ein
letzter Abtastwert und zwei Marken je Kanal). Clippt bei ±0,9549925859,
also rund −0,2 dB, ohne Regler.

Offen, weil es **Herunterladen** von fremdem Quelltext heißt und das
Martins Entscheidung ist. Längst nicht jede Airwindows-Datei lohnt — die
meisten wären Doppelungen zu `ClientComp` und `ClientDeEss`. Diese eine
schließt eine echte Lücke.

### Vorschlag, noch nicht eingebaut: Mehrband

NU9N ist hier unbequem deutlich und spricht gegen den naheliegenden
Wunsch:

> broadcast processors „are not specific enough unless you are
> transmitting at 6 kHz of audio bandwidth or more. Their flexibility is
> very limited below 6 kHz." — <https://www.nu9n.com/tx.html>

Bei 2,3 bis 4,0 kHz ist ein echter Optimod-Mehrbänder also eher Aufwand
als Gewinn. WDSP bringt mit CFC ohnehin schon einen zehnbandigen
Dichteregler mit, den Longpath bereits bedient. **Empfehlung: nicht
bauen**, bis jemand breiter als 6 kHz sendet.

---

## Was ein Profil ist — nach dem Umbau vom selben Tag

**Dynamik. Kein Entzerrer.** `apply()` fasst die Klangregelung nicht
an: nicht gesät, nicht ein-, nicht ausgeschaltet, nicht gelesen. Die
Kurve des Bedieners bleibt, wie sie ist.

Das ist Zeus' Form, am 2026-10-09 am laufenden Gerät abgelesen. Seine
drei Stimmstile sind je drei Angaben:

| | |
|---|---|
| NATURAL | Expander −40 dB (SNR 28 dB), trim +9,5 dB auf −20 dBFS, AGC −22 |
| BROADCAST | Expander −38 dB (SNR 28 dB), trim +9,5 dB auf −20 dBFS, breit auf 2,9 kHz |
| DX PUNCH | Expander −38 dB (SNR 28 dB), trim +9,5 dB auf −20 dBFS, Komp 4:1 |

Expander aus dem gemessenen Störabstand, ein Pegeltrimm auf ein festes
Ziel, genau **eine** kennzeichnende Änderung. Kein Klangregler.

Longpath macht es ebenso:

| Stufe | Woher |
|---|---|
| Expander | Schwelle = Rauschteppich + 0,65 × Störabstand, Boden −8 dB |
| Trimm | auf −20 dBFS, als Drive **vor** dem Kompressor |
| De-Esser | auf der gemessenen Zischfrequenz (`VoiceAnalysis::sibilanceHz`) |
| Kompressor | Verhältnis = gemessener Scheitelfaktor ÷ angezielter (Dichte) |
| Phasendreher | 8 Stufen — Zeus' gemessene Wahl an derselben Stimme |
| Breite | **ist** die kennzeichnende Änderung |

Je Breite unterscheiden sich nur Dichte, Kompression, die Breite selbst
und ob die Farbstufen laufen:

| Breite | Dichte | Farbstufen |
| --- | --- | --- |
| 2,3 k | 75 Dense | **aus** |
| 2,7 k | 55 Broadcast | an |
| 3,2 k | 42 | an |
| 4,0 k | 40 Natural | an |

### Warum der Entzerrer draußen bleibt

Die erste Fassung leitete je Breite einen Hochpass, zwei Kuhschwänze
und eine Präsenzglocke ab. Das Urteil nach dem Hören war eindeutig:

> „Das Original-Audio ist gut, nur sämtliche Bearbeitungen sind eine
> Katastrophe." · „hier ist kein Bass, hier fehlt es einfach an allem."

Vier Fehler, alle derselben Art — Code, der an einem Klangregler dreht,
für den die Messung nicht trägt:

1. **Eine Contest-Kurve für alle vier Breiten.** `VoiceAnalyzer::targetDb`
   verlangt 125 Hz bei −12 dB gegen 1 kHz.
2. **Überhaupt ein absolutes Ziel.** Das Spektrum einer Stimme fällt
   natürlicherweise; 250 Hz liegt bei jeder Stimme weit über 1 kHz.
   Darauf zu zielen bügelt die Neigung flach — gemessen 8,3 dB Körper
   weg beim *breiten* Profil.
3. **Gestapelte Schnitte.** Hochpass beim 1,15- bis 1,8-**fachen** des
   Grundtons (bei 110 Hz also 198 Hz, mitten in der Stimme) und direkt
   darüber ein Tiefenfilter auf dieselbe Gegend.
4. **Die Brummkerbe auf dem Grundton.** `VoiceAnalyzer` summiert ±2
   FFT-Bins um 50/100/150/200 Hz — bei 4096 Punkten und 48 kHz rund
   ±23 Hz. Eine Männerstimme mit 110 Hz legt ihren Grundton in das
   Fenster um 100 Hz und ihre zweite Oberwelle in das um 200. Der
   Detektor meldete Brumm, der nicht da war, und die Kerben nahmen das
   Fundament heraus. Gemessen ohne jeden Brumm: 110 Hz −12,5 dB,
   150 Hz −17,6 dB, 220 Hz −3,7 dB.

Der Detektor selbst ist unverändert — `StripTuner` hängt an derselben
Funktion, das wäre eine eigene Änderung mit eigener Gegenprobe. Der
Optimierer folgt ihm nur nicht mehr.

## Hören, ohne zu entscheiden

Die vier Profile stehen in keiner Kette, also gibt es keinen Abgriff
hinter dem Strip, von dem sie aufzunehmen wären. Sie werden **gerechnet**:
der rohe Durchgang kopiert, durch eine Wegwerf-`StripChain` mit dem Plan
geschoben, aus dem Speicher abgespielt — wie die beiden anderen
Wiedergaben, abseits des Echtzeitpfades.

Je Tastendruck eine frische Kette. Eine wiederverwendete trüge mit, was
der vorige Plan hinterlassen hat, und genau das würde ein A/B zwischen
zwei Profilen zum Vergleich der falschen zwei Dinge machen.

„Ohne" liegt daneben: derselbe Durchgang, unbehandelt, einen Klick
entfernt.

---

## Was der Optimierer nicht tut

* **Nichts anwenden.** `plan()` ist eine reine Funktion der Messung,
  `saveAsPresets()` schreibt benannte Profile. Die Kette ändert sich erst,
  wenn der Bediener eines auswählt. Genau das macht den Parallelbetrieb
  zum bisherigen Weg möglich.
* **Den Hauptschalter nicht anfassen.** Wie überall in `core/strip/`.
* **Das Sendefilter nicht stellen.** Der Plan *trägt* `lowCutHz` /
  `highCutHz` und zeigt sie an; gestellt wird es von dem, der den Plan
  anbietet — sichtbar und mit Absicht. Die WDSP-Sendebandbreite gehört
  `TransmitModel` und ist von `core/strip/` aus bewusst nicht erreichbar.
* **Nichts versprechen.** Jeder Plan nennt seine Kosten, und einer, der
  sich nicht lohnt, sagt das in `caveat`. Ein 4-kHz-Profil aus einem
  Mikrofon, das bei 3,1 kHz schon 20 dB unten ist, ist ein leeres
  Versprechen — und die neue Abfallmessung sieht das.

---

## Prüfstand

`tests/tst_broadcast_optimizer.cpp`, 12 Fälle. Die beiden, auf die es
ankommt:

* **Die K-Gewichtung gegen die veröffentlichte 48-kHz-Tabelle.** Die
  Ableitung muss die fünf Zahlen je Stufe treffen; zufällig geht das
  nicht.
* **Die vier Profile unterscheiden sich wirklich, und in der behaupteten
  Richtung.** Eine Breitentabelle so zu verdrahten, dass jede Zeile
  dieselbe Kette erzeugt, ist das Leichteste der Welt — und sähe im
  Fenster völlig plausibel aus.

Dazu: Lautheit folgt der Verstärkung auf 0,01 dB, LRA sieht die 10-dB-Stufe
und sieht beim Dauerton keine, Stille ist keine Messung, zweimal Anwenden
lässt nichts stehen, der Hochpass folgt der Tonhöhe und bleibt im Deckel,
übersteuert/ungültig wird verweigert.

---

## Offen

* **Live am Gerät noch nicht gesehen.** Bau und Prüfstand ja, ein
  Durchgang mit echter Stimme nein.
* **`ClipOnly2` und der Mehrbänder** — siehe oben, beides Martins
  Entscheidung.
* **Die Stufenzahl des Phasendrehers je Stimme suchen.** Zeus misst sie
  („8 stages measured best") und meldet sie als eine begrenzte
  Änderung mit UNDO daneben; wir nehmen seine 8. Dafür braucht es die
  Aufnahme, die `plan()` nicht hat — ein eigener Schritt.
* **Dichte von Hand verstellen.** Die Achse ist da und wird angezeigt,
  aber noch ohne Regler: `Plan::density` ändern und neu rechnen wäre
  wenige Zeilen. Bewusst nicht mitgebaut, bis die vier Vorgaben an einer
  echten Stimme gehört wurden — sonst ist es ein Regler für eine Skala,
  die noch niemand beurteilt hat.
* **Die vier Dichte-Stufenwerte** sind Longpaths Setzung, nicht Zeus'.
  Wenn Zeus Link sie sichtbar macht, nachziehen.
