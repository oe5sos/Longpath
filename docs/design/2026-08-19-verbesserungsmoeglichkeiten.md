# Was wir verbessern können — eine Liste mit Belegen

Auftrag des Betreibers (2026-08-19): „versuche viele Möglichkeiten zu
finden, was wir verbessern und ändern können."

Jeder Punkt ist im Baum nachgeprüft. Wo ich zuerst etwas vermutet und
dann widerlegt habe, steht es dabei — ein Vorschlag, der auf einer
falschen Beobachtung steht, kostet später mehr als er einbringt.

---

## 1. Apollo und PennyLane: gebaut, an nichts angeschlossen

**Befund.** `ApolloController` und `PennyLaneController`
(`src/core/accessories/`) werden in `RadioModel` gehalten, bekommen beim
Verbinden die MAC-Adresse und laden ihren Zustand
(`RadioModel.cpp:6022-6028`). Danach passiert **nichts**:

* keine Bedienfläche — kein Setup, kein Menü, kein Applet
* **aber sehr wohl Tests**: `tst_apollo_controller.cpp` und
  `tst_penny_lane_controller.cpp`, zusammen 35 Fundstellen. *(Korrektur
  vom selben Tag: hier stand zuerst „kein Testfall". Falsch — ich hatte
  nach dem Zugriffsnamen `apolloController` gesucht statt nach der
  Klasse. Der Befund wird dadurch besser: die Logik ist geprüft, es
  fehlt wirklich nur der Anschluss.)*
* auf der Leitung stehen Nullen:
  `P1RadioConnection.cpp:548` → `out[2] = 0; // mic/apollo flags —
  TODO(3I-T7): wire from state`
* `SettingsHygiene.cpp:125` **löscht** die Apollo-Schlüssel sogar wieder

Das ist derselbe Fall wie der Bandplan und die Modusgruppen, nur älter
und vollständiger: Zustand wird gepflegt, gespeichert, wieder gelöscht —
und wirkt nie.

**Vorschlag.** Entweder anschließen (Setup-Seite unter Hardware plus die
zwei Bits in `out[2]`) oder ausbauen. Weil die Controller getestet sind,
ist der Anschluss die kleinere Arbeit von beiden. Beides ist besser als der
Ist-Zustand, in dem ein Betreiber mit Apollo-Zusatz nicht erkennen kann,
dass die Anwendung ihn ignoriert. *Aufwand: klein bis mittel. Braucht
eine Entscheidung, keine Analyse.*

## 2. 72 gesperrte Regler im Setup

**Befund.** `grep -c NYI src/gui/setup/*.cpp` → **72**, dazu 53
`setEnabled(false)`:

| Seite | gesperrte Regler |
| --- | --- |
| Appearance | 22 |
| Display | 17 |
| Diagnostics | 12 |
| CAT / Netzwerk | 9 |
| Keyboard | 5 |
| DSP | 5 |
| Transmit | 2 |

Das ist das **Gegenstück** zu Punkt 1: dort gebaut und unerreichbar, hier
erreichbar und nicht gebaut. Für den Betreiber ist der zweite Fall
ärgerlicher, weil er die Regler sieht und anfasst.

**Vorschlag.** Durchgehen und je Regler entscheiden: bauen, ausblenden
oder mit einem Hinweis versehen, warum er da ist. Ein gesperrter Regler
ohne Erklärung ist ein Versprechen, das die Anwendung nicht hält.
*Aufwand: pro Regler klein, in Summe groß. Gut als Nebenarbeit.*

## 3. Der S-Verlauf hat zwei Schwellen ohne Bedienfläche

**Befund, und es ist meiner von heute.** `SignalHistoryStore` kennt
`setQrmGateSeconds` (3–30 s, wie lange ein Träger stehen muss, bis er als
Störung gilt) und `setLifetimeSeconds` (15–300 s, wann ein Eintrag
vergeht). Beide sind begrenzt, getestet — und **niemand rief sie**. Beim
Vorbild sind es zwei Schieber im Spot-Hub.

**Erledigt am selben Tag:** zwei Zahlenfelder unter Setup → Display →
Spectrum Overlays („Interference after" und „Remember stations for"),
durchgereicht bis `SignalHistoryStore`, mit Persistenz und Rückweg beim
Öffnen der Seite. Die **Grenzen bleiben in der Klasse** — eine zweite
Bedienfläche würde sie sonst umgehen; drei Testfälle halten das fest.

## 4. Der Rechtsklick dreht, der Doppelklick nicht

**Befund.** Seit heute gibt es zwei Wege von einem Spot zum Rotor:

* Rechtsklick → „Turn rotor to <call>" → `workSpot()` → **dreht den Mast**
* Doppelklick → `takeSpot()` → Log auf, Zeiger auf Ziel, **dreht nicht**

Beides ist so gewollt (der Doppelklick soll den Vergleich Ist gegen Ziel
zeigen), aber zwei ähnliche Gesten mit verschiedenem Ausgang sind eine
Stolperstelle.

**Vorschlag.** Beim Betreiber nachfragen, ob der Doppelklick zusätzlich
drehen soll — und wenn nein, den Unterschied im Rechtsklick-Menü
benennen („Turn rotor to …" gegen „Log …"). *Aufwand: winzig, sobald die
Frage beantwortet ist.*

## 5. Ein Werkzeug statt Wachsamkeit

**Befund.** An zwei Tagen sind **sieben** „Merkmale" als Fehlalarm
aufgeflogen und **drei** Merkmale als „gebaut, an keiner Fläche"
(Modusgruppen, Bandplan, jetzt Apollo/PennyLane). Beides fand ich von
Hand. Das ist Glück, kein Verfahren.

**Gebaut am selben Tag:** `scripts/find-unsurfaced-features.py`. Es
meldet mechanisch:

1. Klassen in `src/core` und `src/models`, die in `src/gui` **nicht
   einmal** vorkommen (bei mir heute: 45 Treffer, davon die meisten
   berechtigt — Codecs, Audio-Quellen, Arbeiter; die Ausnahmeliste
   gehört ins Skript).
2. Öffentliche Setter, die außer in Tests **nirgends** gerufen werden.

Als Bericht, nicht als Sperre: die Regel „jedes Merkmal braucht eine
Fläche" hat berechtigte Ausnahmen, und ein Tor, das ständig zu Unrecht
rot wird, wird abgeschaltet. Die Ausnahmeliste im Skript führt jede
Klasse mit Begründung — eine Liste ohne Gründe wächst, bis sie alles
enthält.

Erster Lauf: **2 Treffer**, nämlich genau Apollo und PennyLane aus
Punkt 1. Die 43 anderen Klassen ohne GUI-Vorkommen stehen begründet in
der Ausnahmeliste (Codecs, Audio-Quellen, Transport-Arbeiter,
TCI-Innereien).

## 6. Kleinkram, belegt

* **`ContainerCount`** (`ContainerManager.cpp:612`) wird bei jedem
  Speichern geschrieben und **nirgends gelesen**. Verwaister Schlüssel.
* **TODO-Dichte:** 67 in `src/core`, 38 in `src/gui`, 4 in `src/models`.
  Kein Alarm, aber `TODO(3I-T7)` aus Punkt 1 zeigt, dass mindestens einer
  davon ein stillgelegtes Merkmal verdeckt. Eine Durchsicht der
  `TODO(<Phase>)`-Marken würde weitere finden.

## Was ich geprüft und verworfen habe

* **Rauschboden-Farbschlüssel** (`DisplayNoiseFloorColor` und zwei
  weitere) sahen nach „geschrieben, nie gelesen" aus. Falsch: sie werden
  über eine Hilfsfunktion gelesen (`SpectrumWidget.cpp:1181-1185`). Meine
  Suchmuster erfassten den Lesezugriff nicht.
* **„Nur gelesen, nie geschrieben" (248 Schlüssel)** ist als Liste
  wertlos: die meisten Schreibzugriffe laufen über `writeInt`,
  `writeBool` oder `settingsKey(...)`. Wer diese Prüfung ernsthaft will,
  muss die Hilfsfunktionen mitverfolgen.
* **Das Rotor/Log-Panel und die Farben.** Der Betreiber wünschte, das
  Layout an unsere Farben anzupassen. Das Panel benutzt bereits durchweg
  die Palette (46 `Style::`-Zugriffe, kein einziger harter Farbwert).
  Anzupassen war etwas anderes: die S-Verlauf-Marken, die ich heute mit
  den Farbwerten des Vorbilds eingebaut hatte — die laufen jetzt über
  `roleColor("warn")` und `roleColor("danger")`.

---

## Modification history (NereusSDR)

* 2026-08-19 — angelegt von Martin Fischer, KI-gestützt über Anthropic
  Claude (Cowork).
