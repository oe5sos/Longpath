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

## Der Auftrag

Martin, 2026-10-09, in drei Nachrichten:

> „stimme analysieren, verbessern, optimieren, möglichkeiten aufzeigen und
> daraus resultierend ein 2,3 – 2,7 – 3,5 und 4 k Profil zu erstellen"

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

## Die Entscheidungen, Spalte für Spalte

`kRules` in `BroadcastOptimizer.cpp` — eine Zeile je Breite, damit die
vier Unterschiede an einer Stelle lesbar sind statt über vier Zweige
verteilt.

| Breite | Hochpass | Deckel | Tiefe extra | Höhen extra | Dichte | Klangstufen |
| --- | --- | --- | --- | --- | --- | --- |
| 2,3 k | 1,80 × Grundton | 300 Hz | −3,0 dB | −9,0 dB | 75 Dense | **aus** |
| 2,7 k | 1,60 × | 250 Hz | −2,0 dB | −7,0 dB | 55 Broadcast | an |
| 3,5 k | 1,30 × | 180 Hz | −0,5 dB | −3,0 dB | 42 | an |
| 4,0 k | 1,15 × | 150 Hz | 0 | −1,5 dB | 30 Natural | an |

**Der Hochpass folgt dem Grundton, nicht einer festen Zahl.** Dafür misst
`VoiceAnalyzer` jetzt die Tonhöhe (Autokorrelation je Rahmen, Median über
die Rahmen mit klarer Spitze). Bisher wurde der Hochpass aus der Energie
unterhalb des Sprachbandes gewählt — die richtige Frage, falsch herum
gestellt: dieselben 40 Hz Rumpeln liegen unter einer 95-Hz- und einer
185-Hz-Stimme, aber ein Filter, das für die eine stimmt, nimmt der anderen
die unterste Oktave.

**Die Präsenz wird nie angehoben.** Sie muss nicht: Tiefen und Höhen
ringsum abzusenken *ist* „nach vorne", und es kostet keine Verstärkung auf
einem Band, auf dem das Mikrofon rauschen könnte. Gleiche Regel wie in
`VoiceAnalyzer::suggestEq` und `StripTuner` — nur Absenkungen, der Pegel
kommt als Aufholverstärkung zurück.

**Das Verhältnis wird gelöst, nicht gewählt.** Steht die Schwelle auf dem
Sprachmittel, liegen die Spitzen den gemessenen Scheitelfaktor darüber und
kommen mit Scheitelfaktor/Verhältnis heraus. Das Verhältnis, das auf den
Zielscheitelfaktor führt, ist also gemessen ÷ Ziel. Nichts daran ist
Geschmack.

**Der De-Esser läuft auch bei 2,3 k** — der naheliegende Einwand ist, ein
2,3-kHz-Filter entferne 6 kHz ohnehin. Tut es, aber der Strip steht **vor**
dem Sendefilter: das Zischen erreicht diesen Kompressor zuerst und nimmt
Pegelabsenkung mit. Genau so holen die schmalen Profile ihren Spielraum
zurück.

### Was die schmalen Profile vergessen

Martins Regel, und sie hat eine veröffentlichte Entsprechung: NU9Ns
Kettenvorschlag kennzeichnet Rauschsperre, weichen Kompressor, Exciter,
Spitzenbegrenzer und Effekte ausdrücklich als **optional** — unmarkiert
bleiben genau zwei, Entzerrer und Kompressor.

Das 2,3-k-Profil schaltet Röhre, Exciter und Nachhall darum **ab**, statt
sie zu lassen, wie sie waren. Die Unsymmetrie ist Absicht: eingeschaltet
wird nie etwas (dafür gibt es keine Messung), ausgeschaltet schon. Ein
Profil, das sich schlank nennt, während der Nachhall des vorigen noch
läuft, ist eines, dem man nicht trauen kann — und ein A/B zwischen zwei
Profilen muss den Unterschied *zwischen ihnen* hörbar machen.

---

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
* **Dichte von Hand verstellen.** Die Achse ist da und wird angezeigt,
  aber noch ohne Regler: `Plan::density` ändern und neu rechnen wäre
  wenige Zeilen. Bewusst nicht mitgebaut, bis die vier Vorgaben an einer
  echten Stimme gehört wurden — sonst ist es ein Regler für eine Skala,
  die noch niemand beurteilt hat.
* **Die vier Dichte-Stufenwerte** sind Longpaths Setzung, nicht Zeus'.
  Wenn Zeus Link sie sichtbar macht, nachziehen.
