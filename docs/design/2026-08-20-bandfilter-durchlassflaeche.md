# Bandfilter: eine eigene Durchlass-Bedienfläche

**Stand:** 20. August 2026, Nachtschicht
**Quelle:** `../Thetis` @ `v2.10.3.15-5-g852bf0e`,
`Project Files/Source/Console/FilterForm.cs` (969 Zeilen)
**Anlass:** „vielleicht kannst du mal schauen, welche möglichkeiten es
für einen band filter gibt, halte dich an zeus" — notiert als Todo ohne
Eile.

**Dies ist ein Vorschlag, nichts davon ist gebaut.** Eine neue
Bedienfläche ist eine Gestaltungsentscheidung; die gehört dir, nicht
mir.

---

## Was wir schon haben

Mehr als ich erwartet hatte:

* **Filterkanten am Panadapter ziehen.** `SpectrumWidget.cpp:8148-8163`,
  ±5 px Fanggrenze, beide Kanten, mit `filterEdgeDragged(low, high)`.
  Portiert aus AetherSDR.
* **Innerhalb des Durchlasses ziehen** verschiebt bei uns den VFO
  (`m_draggingVfo`) — AetherSDR-Verhalten.
* **`FilterDisplayItem`** als Instrument im Baukasten: es *zeigt* den
  Durchlass, bedienen kann man es nicht.

## Was Thetis hat und wir nicht

| Thetis | Wir |
| --- | --- |
| **Eigenes kleines Fenster** nur für den Durchlass, unabhängig vom Panadapter | nichts |
| Ziehen **innerhalb** verschiebt den **ganzen Durchlass** (beide Kanten, VFO bleibt) — `drag_filter`, FilterForm.cs:894-900 | wir verschieben dort den VFO |
| **Breiten-Feld** mit Regeln je Betriebsart — FilterForm.cs:918-960 | nichts |
| Zeigercursor sagt, was passiert: `SizeWE` an der Kante, `NoMoveHoriz` innen | nur `SizeHorCursor` |
| Benannte Filter anlegen, umbenennen, löschen (`frmFilterManager.cs`) | nichts |

### Die Breiten-Regeln, weil sie der eigentliche Gewinn sind

Eine Zahl eintippen und die Kanten setzen sich **richtig** — je nach
Betriebsart anders (FilterForm.cs:924-957):

| Modus | Regel |
| --- | --- |
| CWL / CWU | zentriert auf ∓CW-Pitch: `low = ∓pitch − breite/2` |
| DIGL / DIGU | zentriert auf den Click-Tune-Versatz (`//W4TME`) |
| LSB | oben verankert bei `−DefaultLowCut`, wächst nach unten |
| USB | unten verankert bei `DefaultLowCut`, wächst nach oben |
| AM / SAM / FMN | symmetrisch um null |

Das ist der Unterschied zwischen „2,4 kHz" eintippen und zwei Kanten
von Hand suchen. Bei CW ist es mehr als Bequemlichkeit: ein Durchlass,
der nicht auf dem Pitch sitzt, lässt den Ton wandern, wenn man ihn
schmaler zieht.

## Vorschlag

**Ein Streifen, drei Ziehflächen, ein Zahlenfeld.**

```
┌──────────────────────────────────────────────────┐
│ BANDWIDTH                          2.4 kHz  ▲▼   │
├──────────────────────────────────────────────────┤
│          ┌───────────────────┐                   │
│          │███████████████████│         ← ziehen  │
│          └───────────────────┘                   │
│  −3k        −300        0        +300      +3k   │
└──────────────────────────────────────────────────┘
     ↑ Kante          ↑ ganzer Durchlass      ↑ Kante
```

* **Kante ziehen** — eine Seite, wie am Panadapter.
* **Mitte ziehen** — den ganzen Durchlass verschieben, VFO bleibt
  stehen. Das ist der Griff, der uns fehlt: bei CW schiebt man den
  Durchlass über eine Störung, ohne die Frequenz zu verlieren.
* **Zahlenfeld** — Breite eintippen, Kanten setzen sich nach der Regel
  der Betriebsart.
* **Zeigercursor sagt es vorher**: Doppelpfeil an der Kante,
  Verschiebe-Zeichen in der Mitte.

**Wohin damit** — drei Möglichkeiten, ich würde die erste nehmen:

1. **In den RxApplet**, unter die Filterknöpfe. Man ist ohnehin dort,
   wenn man am Filter arbeitet, und es braucht kein zweites Fenster.
2. Als eigene Kachel (mit dem Kachelrahmen von heute), für alle, die
   den Durchlass ständig sehen wollen.
3. Als Fenster wie bei Thetis — der Weg, den ich am wenigsten mag: ein
   Fenster, das man öffnen und schließen muss, wird nach der zweiten
   Woche nicht mehr geöffnet.

**Was NICHT dazugehört**, obwohl Thetis es hat: den Durchlass-Streifen
zusätzlich auf dem Panadapter zu bedienen. Dort ziehen die Kanten
bereits, und die Mitte gehört dem VFO. Zwei Bedeutungen für dieselbe
Geste an derselben Stelle wären genau die Doppelung, die wir heute
zweimal aufgeräumt haben.

## Aufwand, ehrlich geschätzt

* Streifen + drei Ziehflächen + Cursor: überschaubar, prüfbar ohne
  Funkgerät (Ziehen ist Mausereignis rein, Hz raus).
* Breiten-Regeln je Betriebsart: braucht `CWPitch` und `DefaultLowCut`
  aus unserem Modell — beide sind da, aber der Weg dahin will
  nachgesehen werden.
* Benannte Filter (`frmFilterManager`): eigener Schritt, nicht Teil
  davon.

**Sag, welche der drei Stellen dir passt, dann baue ich es.**
