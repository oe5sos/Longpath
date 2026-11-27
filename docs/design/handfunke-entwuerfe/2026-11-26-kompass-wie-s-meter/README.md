# Rotor-Kompass im Stil des S-Meters (Vorschläge, 2026-11-26)

Martin: „die Grafik Rotor im Vergleich zu S-Meter gefällt mir nicht, sollte angepasst
werden." Echte Seite gegen `attrappe.py --rotorstand 255`. Heute: [jetzt-iphone-393.png](jetzt-iphone-393.png)
(Punktzifferblatt nach RotatorItem, dünne Nadel).

| Blatt | Idee |
| --- | --- |
| A — Vollkreis | Rille rundum wie beim S-Meter, Teilstriche alle 10°/30°/90°, N O S W, derselbe Zeiger mit Glanzkante und Nabe, Glut in der Mitte |
| B — Halbbogen 180°→360° | exakt die Form des S-Meters (gleiche Zeichnung), nur der nutzbare Bereich S – W – N; Fläche bis zum Zeiger gefüllt. Steht der Rotor im Osten (gesperrt), bleibt der Zeiger am Rand |
| C — Vollkreis mit gesperrtem Osten | wie A, der Bereich 0°–180° (Berg) rötlich wie die SWR-Schwelle |

Nichts an der App geändert; gebaut wird nach Martins Wahl.

## Gewählt: A (Martin, 27.11.)

Gebaut in `instrument.js` `kompassRund()`; `gebaut-*.png` zeigen die App gegen `attrappe.py --rotorstand 255`.
