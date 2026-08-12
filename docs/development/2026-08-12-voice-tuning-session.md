# Voice-Tuning-Session — Plan für 2026-08-12 (60 Minuten)

Vorbereitet in der Nacht auf den 12., damit die eine verfügbare Stunde
komplett ins Hören geht statt ins Suchen. Gegenstück im Code: die
Stage-„Characters" in `src/core/strip/StripCharacters.cpp` — jede
Stufe hat benannte Startpunkte mit einem Satz Begründung; der Plan
unten verweist auf genau diese Namen.

## Minutenplan

| Zeit | Schritt |
|---|---|
| 0-5 | App starten. **Span auf 48 kHz stellen** (aktive Bänder — persistiert pro Band). Kurz prüfen: idle IQ-Audit zeigt die konfigurierte Rate, MOX ändert die Paketrate nicht mehr (der Connect-Fix von heute Nacht). |
| 5-10 | Channel Strip öffnen → Voice-Check-Tab. **Referenzaufnahme A**: Strip-Master AUS, ● Record, 15 s Standardtext (unten), anhören. Das ist die ehrliche Null-Linie. |
| 10-15 | Strip-Master AN, alle Stufen auf die „Startbild"-Charaktere (unten). **Aufnahme B**, anhören, mit A vergleichen. |
| 15-45 | Iterieren nach der Beschwerde-Tabelle (unten): pro Runde EINE Änderung, neu aufnehmen, anhören. 4-6 Runden sind realistisch. |
| 45-55 | Feintuning der zwei Stufen, die den größten Unterschied gemacht haben. Finale Aufnahme. |
| 55-60 | Einstellungen sind automatisch persistiert. Kurznotiz, was gewonnen hat (fürs Log / für den nächsten Mic-Wechsel). |

**Standardtext** (deckt Zischlaute, Plosive, tiefe Vokale):
„Sechs tschechische Schüler zeichnen sich durch besondere Sprüche aus.
Paris, Bordeaux, Population. Morgen wird ein wunderbarer Tag, ohne
Wolken, ohne Wind." Plus das eigene Rufzeichen, zweimal, normal.

## Startbild (Aufnahme B)

| Stufe | Character | Warum |
|---|---|---|
| Gate | **SSB** | 3:1, Floor 18 dB, langer Hold — atmet nicht zwischen Wörtern. |
| EQ | flach lassen | Erst hören, DANN schrauben. |
| De-Esser | **Wide** | Breites Band um 6 kHz als Ausgangspunkt. |
| Kompressor | **Balanced** | 3:1, hörbar dichter, noch nicht verfärbt. |
| Tube | **Warm** | 5 dB Drive, ein Drittel Mix. |
| Pudu | **Both** | Wärme + Präsenz, moderat. |
| Reverb | **AUS** | Für SSB-Verständlichkeit fast immer kontraproduktiv; nur bewusst einsetzen. |
| Final Limiter | **Safety** | Ceiling −3 dB, fängt Ausreißer. |

## Beschwerde → Dreh

| Höreindruck | Erste Maßnahme | Zweite Maßnahme |
|---|---|---|
| Zu dumpf / „Decke drüber" | EQ: Anhebung 2-4 dB um 2-4 kHz | Pudu **Presence** statt Both; Tube-Mix runter |
| Zu spitz / anstrengend | EQ: 2-4 kHz absenken | Tube **Warm** Mix rauf; Pudu **Warmth** |
| Zu wenig Bass / dünn | EQ: +2-3 dB um 120-250 Hz | Pudu **Deep voice** (70-Hz-Generator) |
| Zu viel Bass / wummert | EQ: Low-Cut ~100 Hz, −3 dB um 200 Hz | Tube-Mix runter |
| S-Laute zischen | De-Esser **Narrow**, Threshold tiefer | Bei tiefer Stimme: **Lower voice** (5 kHz) |
| Näselt (~1 kHz Quäke) | EQ: −2-4 dB schmal um 800-1200 Hz | — |
| Blechdose / hohl | EQ: −3 dB um 400-600 Hz | Reverb sicher AUS |
| Zu leise / setzt sich nicht durch | Kompressor **Contest** | Limiter **Broadcast**; NIE zuerst am Limiter drehen |
| Verzerrt / überfahren | Tube-Mix runter, Kompressor **Gentle** | Limiter **Transparent**, Eingangs-Gain prüfen |
| Hintergrund hörbar (Lüfter etc.) | Gate **Shack fan** | Threshold nachziehen, bis Sprechpausen still sind |

## Regeln für die Stunde

1. **Eine Änderung pro Aufnahme.** Zwei gleichzeitig = nichts gelernt.
2. Nach jeder dritten Runde einmal **A/B gegen die Roh-Aufnahme** —
   Ohren gewöhnen sich in Minuten an jede Färbung.
3. Reverb und „Voodoo"-Charaktere sind Vorführ-Effekte, keine
   Betriebseinstellungen.
4. Der Voice-Check hört das PC-Mikrofon. Die Kette Radio-Mikrofon →
   TXA bleibt davon unberührt; was hier gewonnen wird, gilt für den
   PC-Mic-/VAX-Pfad und die Strip-Stufen selbst.

## Falls Zeit übrig bleibt (Netzwerk-Beweis, 5 Minuten)

```
~/Desktop/neureus/NereusSDR/build/NereusSDR.app/Contents/MacOS/NereusSDR 2>&1 | grep --line-buffered -E "seq audit|mic reorder"
```

Mit 48-kHz-Span: verbinden, 60 s idle, 60 s MOX. Erwartung: idle
~1010 pkts/5 s, unter MOX UNVERÄNDERT, Verluste im Idle-Niveau
(0,1-0,5 %, überwiegend „rescued"). Damit wäre die Bench-Zeile des
Connect-Fixes grün und der Netzwerk-Fall komplett geschlossen.
