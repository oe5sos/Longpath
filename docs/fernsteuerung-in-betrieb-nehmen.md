# Fernsteuerung in Betrieb nehmen

Wie ein fremdes Programm — Contestlog, WSJT-X, fldigi — Longpath steuert.
Zwei Wege stehen bereit; beide hängen am selben Gerät und schließen sich
nicht aus.

| | rigctld | CAT (Kenwood) |
| --- | --- | --- |
| Port | 4532 | 13013 |
| Wer spricht es | N1MM, DXLog, WSJT-X, fldigi, alles mit Hamlib | ältere Programme und alles, was auf Thetis eingerichtet ist |
| Im Programm einzutragen als | „Hamlib NET rigctl" | Kenwood TS-2000 über TCP |

## In Longpath einschalten

**rigctld:** Setup → **CAT & Network → TCP/IP CAT**.
**Kenwood-CAT:** im **CAT-Applet** (dort sitzen TCP und Pseudoterminal) —
nicht in Setup; ein Schalter an zwei Orten laeuft auseinander.

1. Haken bei *Enable rigctld server* bzw., im Applet, bei *TCP*.
2. **Bind IP auf 127.0.0.1 lassen**, solange das steuernde Programm auf
   demselben Rechner läuft. 0.0.0.0 öffnet den Port für das ganze Netz —
   dann kann alles, was den Port erreicht, die Frequenz verstellen.
3. Das Senden bleibt **gesperrt**. Die Haken *Allow transmit* stehen
   bewusst getrennt und standardmäßig aus.
4. Die Statuszeile darunter sagt, ob der Server horcht und wie viele
   Programme hängen (rigctld; das Applet zeigt dasselbe für CAT).

Die Einstellungen wirken sofort, ohne Neustart.

## Im steuernden Programm

**N1MM / DXLog:** Gerät „Hamlib NET rigctl", Adresse `127.0.0.1:4532`.
Split auf **„None" oder „Fake It"** stellen — Longpath hat kein Split
(dazu unten mehr).

**WSJT-X:** Settings → Radio → Rig: *Hamlib NET rigctl*, Network Server
`127.0.0.1:4532`. PTT Method: **VOX**, solange das Sendetor zu ist; auf
*CAT* erst umstellen, wenn du *Allow transmit (PTT) over rigctld*
eingeschaltet hast. „Test CAT" muss grün werden, bevor du weitergehst.

**fldigi:** Configure → Rig Control → Hamlib, Port `127.0.0.1:4532`.

## Was geht, und was bewusst nicht

Lesen und Stellen von **Frequenz**, **Modus mit Durchlassbreite**, VFO,
Sperre, S-Meter und der Feinabstimmung (RIT/XIT). Der Statussatz `IF;`
trägt alles auf einmal, so wie ein TS-2000 ihn schickt.

**Split gibt es nicht.** Longpath hat keinen Split-Schalter; der
dokumentierte Ersatz ist XIT oder eine zweite Scheibe. Ein Programm, das
Split verlangt, bekommt eine ehrliche Absage — stell es dort auf „Fake
It", dann tut es dasselbe über die Frequenz.

**Das Senden** nimmt denselben Weg wie jede andere Sendequelle, mit dem
Bandplan-Wächter davor. Entkeyen geht immer, auch bei gesperrtem Tor;
und wenn du den Server abschaltest, ist das Tor beim nächsten Start
wieder zu.

Was ein Programm verlangt und nicht bekommt, wird abgewiesen statt
erfunden: Filterbreiten, Speicherplätze, Squelch, der ganze ZZ-Satz und
die unaufgeforderten Statusmeldungen (`AI1`). Ein Client, der eine
Absage bekommt, fragt ab — das funktioniert.

## Wenn es klemmt

**Prüfen, ob der Server überhaupt antwortet** (ohne das Programm):

```bash
rigctl -m 2 -r 127.0.0.1:4532 f
```

Das muss die Frequenz in Hz ausgeben. Für den CAT-Weg:

```bash
printf 'ID;FA;' | nc 127.0.0.1 13013
```

Antwort `ID019;FA00014250000;` — Gerätekennung und Frequenz.

**Im Protokoll nachsehen.** Die beiden Server schreiben unter
`longpath.rigctld` und `longpath.cat`; dort steht, wer sich verbunden
hat, welcher Befehl kam und warum ein Tastversuch abgelehnt wurde.

**Antwortet nichts**, obwohl der Haken gesetzt ist: Steht in der
Statuszeile „stopped", ist der Port belegt — meist von einem echten
`rigctld`, das noch läuft. Dann entweder das beenden oder in Longpath
einen anderen Port wählen.

**Die Frequenz lässt sich nicht setzen** und der Client meldet einen
Fehler: Das ist richtig so, wenn kein Funkgerät verbunden ist. Beide
Brücken lesen nach jedem Setzen zurück und melden einen Fehlschlag,
statt Erfolg zu behaupten — ein Hamlib-Client würde das sonst nicht
merken, weil er seine eigene Frequenz aus dem Zwischenspeicher liest.

## Stand

Gegen echte Clients geprüft (hamlibs `rigctl` und sein TS-2000-Treiber),
aber **noch nie am Funkgerät**. Die beiden Prüfprotokolle mit den
Drahtmitschnitten liegen unter
`docs/architecture/2026-10-11-rigctld-verification/` und
`docs/architecture/2026-10-11-cat-verification/`.
