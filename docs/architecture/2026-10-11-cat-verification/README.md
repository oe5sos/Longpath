# Kenwood-CAT über TCP — Prüfprotokoll

Datum: 2026-10-11 · Fassung: `fix/discovery-quietpolls-thetis-parity`
Gegenstellen: ein eigener Rohclient (sendet Befehle, liest alles zurück)
**und** hamlibs echter Kenwood-Treiber (`rigctl -m 2014`, also TS-2000,
über TCP). Longpath offscreen mit eigener Konfiguration, CAT auf Port
13113, **ohne Funkgerät**.

## Was der Rohclient sieht

```
gesendet : ID;FA;FA00014250000;MD;IF;PS;FR;FT;AI;AI1;TX;RX;ZZSW;
empfangen: ID019;FA00000000000;?;MD2;IF000000000000000+0000000000020000000;PS0;FR0;FT0;AI0;?;?;?;
```

Zeile für Zeile:

| Befehl | Antwort | warum so |
| --- | --- | --- |
| `ID;` | `ID019;` | TS-2000, wie Thetis' Vorgabe |
| `FA;` | `FA00000000000;` | ohne Funkgerät gibt es keinen Slice — 0 ist die Wahrheit |
| `FA00014250000;` | `?;` | **abgelehnt, weil es nicht wirken konnte** (siehe unten) |
| `MD;` | `MD2;` | USB, der Standardmodus |
| `IF;` | 35 Byte | Feldbreiten geprüft: 11 Frequenz, 4 Schritt, 6 Feinabstimmung, RIT, XIT, 3 Dummy, TX, Modus, 2 Dummy, Split, 4 Dummy |
| `PS;` | `PS0;` | nicht verbunden; `PS1;` wäre eine Behauptung |
| `FR;` / `FT;` | `FR0;` / `FT0;` | Empfang und Senden auf VFO A |
| `AI;` / `AI1;` | `AI0;` / `?;` | wir schieben keine Meldungen, also soll der Client abfragen |
| `TX;` | `?;` | Sendetor zu |
| `RX;` | (nichts) | Entkeyen ist immer erlaubt und antwortet still, wie bei Thetis |
| `ZZSW;` | `?;` | nicht portiert — ehrliche Fehlantwort statt Stille |

## Was hamlibs eigener Kenwood-Treiber sieht

```
rigctl -m 2014 -r 127.0.0.1:13113
f  ->  0
m  ->  USB 2200
t  ->  0
v  ->  VFOA
```

Ein echter TS-2000-Treiber liest unsere Antworten also fehlerfrei,
Modus und Durchlassbreite eingeschlossen.

## Befund, der auch die rigctld-Brücke betraf

`FA00014250000;` wird abgelehnt, weil der Parser nach dem Setzen
**zurückliest** und merkt, dass nichts angekommen ist (ohne Funkgerät
gibt es keinen Slice). Genau dieser Rückblick hat einen Fehler im
rigctld-Protokoll desselben Tages aufgedeckt:

> Der frühere rigctld-Lauf schien zu zeigen, dass `F 14250000` wirkt —
> `f` antwortete danach `14250000`. Das war **hamlibs eigener
> Frequenzcache**: derselbe Moment, über CAT gefragt, ergab 0.
> Gegenprobe gefahren: `F 18100000` über rigctld, `f` sagt 18100000,
> `FA;` über CAT sagt im gleichen Moment `FA00000000000;`.

Die rigctld-Brücke liest jetzt ebenso zurück und antwortet `RPRT -1`,
wenn das Setzen nicht gegriffen hat. Ein Client bekommt damit die
Wahrheit, statt sich auf seinen Cache zu verlassen. Der Satz im
rigctld-Protokoll, Frequenz und Modus seien dort gesetzt worden, gilt
für den unverbundenen Zustand **nicht** und ist dort richtiggestellt.

## Das Sendetor

Eigener Schalter, getrennt vom rigctld-Tor (`CatServerAllowTx`), Standard
zu. `TX;` antwortet dann `?;`, `RX;` geht immer. `stop()` sperrt wieder.
Auch der Weg über TCI (`run_cat_ex:TX;`) bleibt gesperrt: der CAT-Parser
in TciProtocol hat sein eigenes Tor, das nie geöffnet wird.

## Nachtrag am selben Tag: S-Meter und RIT/XIT

Dazugekommen sind `SM` (Kenwood-Skala 0..30 aus dBm, Thetis' zweiteilige
Formel samt Klemmen), `RT`/`XT` (RIT/XIT ein und aus), `RC` (Ablage
löschen) und `RU`/`RD` (Ablage schrittweise verstellen) — die
Feinabstimmung ist bei uns der dokumentierte Ersatz für Split, also
gehört sie an die Fernsteuerung. Auf der rigctld-Seite liefert
`l STRENGTH` jetzt dB über S9.

Live, ohne Funkgerät:

```
gesendet : SM0;RT;RT1;RT;RU;RU;IF;RC;SM1;
empfangen: SM00000;RT0;  (RT1 still)  RT0;?;?;IF...;?;?;
```

Daraus kam der nächste Befund derselben Art: **`RT1;` antwortete still,
obwohl der Schalter nicht umgelegt wurde** — ohne Slice gibt es kein
RIT. Ein Contestprogramm hätte geglaubt, seine Ablage sei an. `RT` und
`XT` lesen jetzt ebenso zurück und antworten `?;`, wenn es nicht
gegriffen hat (Prüffall `rit_switch_that_cannot_take_is_refused`,
gegen ein echtes RadioModel mit und ohne Slice). `SM0;` → `SM00000;`
und `l STRENGTH` → `RPRT -11` sind in diesem Zustand die richtigen
Antworten: es gibt keinen Messwert, und eine erfundene Zahl sähe wie
ein schwaches Signal aus.

## Was offen ist

- Am Funkgerät ist nichts davon gefahren. Mit Slice müssen Setzen von
  Frequenz und Modus, die IF-Zeile unter Last und das Tasten über CAT
  noch geprüft werden.
- Nicht portiert: der ganze `ZZ`-Satz, Filterbreiten (`FL`, `SH`,
  `SL`), Speicherplätze, Squelch (`SQ`). Alles davon antwortet `?;`.
- `AI1` (unaufgeforderte Meldungen) fehlt bewusst; ein Client, der `?;`
  bekommt, fragt ab — das funktioniert.
- Noch nicht gegen N1MM oder DXLog selbst geprüft, nur gegen den
  Rohclient und hamlibs TS-2000-Treiber.
