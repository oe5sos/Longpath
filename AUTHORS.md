# Autoren und Urheberrecht

Longpath steht unter der **GNU General Public License v3** (siehe
[`LICENSE`](LICENSE)). Die GPL verlangt nicht nur, dass der Quelltext
offen ist — sie verlangt auch, dass die Urheber genannt bleiben. Diese
Datei sagt, wer woran Urheberrecht hat.

## Longpath

**Copyright © 2026 Martin Fischer, OE5SOS**

Longpath ist ein Fork von NereusSDR, aber kein bloßer Umbau: 590 der
Quelldateien sind Longpath-eigene Arbeit und keine Portierung —
gemessen am Baum, nicht geschätzt. Dazu gehören unter anderem

- der native SunSDR2-QRP-Treiber (ohne ExpertSDR2 dazwischen),
- der native GS-232-Rotortreiber, seriell und über Netzserver,
- der native FT8/FT4-Dekoder (ft8_lib) samt Ablauf und Bedienung,
- der RTTY- und der CW-Dekoder,
- die freie Arbeitsfläche, die Layoutprofile, die Kommandoleiste,
- die Werkbank für den Sendeton samt Lautheitsmessung nach BS.1770,
- Logbuch, Kennzahlen und Awards,
- das Prüfwerk: über tausend Prüfstände und fünfzehn
  Konformitätsskripte, die Herkunft und Urheberrecht mechanisch
  durchsetzen.

Die Arbeit entstand KI-gestützt über Anthropic Claude; die
Entscheidungen, die Messungen am Gerät und die Abnahme sind Martin
Fischers. Das ist in den Historienblöcken der einzelnen Dateien
festgehalten (`Modification history (Longpath)`).

## Woher Longpath kommt

Longpath trägt die Arbeit anderer weiter, und deren Vermerke bleiben
unangetastet — in jeder einzelnen Datei und in den Herkunftstabellen
unter [`docs/attribution/`](docs/attribution/).

| | |
| --- | --- |
| **NereusSDR** | Copyright © J.J. Boyd, KG4VCF — C++20/Qt6-Portierung, Paketierung, CI |
| **Thetis / PowerSDR** | FlexRadio Systems, Doug Wigley W5WC, Richard Samphire MW0LGE und die OpenHPSDR-Gemeinschaft |
| **WDSP** | Warren Pratt, NR0V |
| **weitere** | AetherSDR, freedv-gui, ft8_lib (Kārlis Goba), rade, ggmorse, rnnoise, r8brain — je mit eigener Lizenz, siehe `docs/attribution/` |

Die vollständige Namensliste steht im Über-Dialog des Programms und
stammt dort verbatim aus Thetis' eigenem `frmAbout`.

## Für Beiträge

Wer etwas beiträgt, behält sein Urheberrecht und trägt sich hier ein.
Eine Copyright-Übertragung wird nicht verlangt. Neue Dateien bekommen
eine eigene Copyright-Zeile; bei Portierungen bleiben die Vermerke der
Quelle byte-genau erhalten — das setzt
`scripts/verify-thetis-headers.py` bei jedem Commit durch.
