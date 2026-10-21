# rigctld-Brücke — Prüfprotokoll

Datum: 2026-10-11 · Fassung: `fix/discovery-quietpolls-thetis-parity`
Gegenstelle: **echter Hamlib-Client** (`rigctl -m 2`, Hamlib 4.7.2,
2026-06-21, SHA 40f63488f) gegen Longpath, offscreen gestartet mit
eigener Konfiguration (`LONGPATH_CONFIG_DIR`, Port 4598), **ohne
Funkgerät**.
Vergleichsgegenstelle: `rigctld -m 1` (Hamlibs eigenes Dummy-Gerät),
dazwischen ein mitlesender TCP-Proxy — so ist das Protokoll nicht
geraten, sondern abgelesen.

## Woher das Protokoll kommt

Nichts davon ist aus Hamlib-Quellen übernommen. Die Befehlsfolge, die
ein Client beim Verbinden fährt, wurde am Draht mitgeschnitten:

```
C>S \get_powerstat      S>C 1
C>S \chk_vfo            S>C 0
C>S \dump_state         S>C <Block, endet auf "done">
C>S v                   S>C VFOA
C>S l KEYSPD            S>C 0
C>S f                   S>C 145000000
C>S \get_lock_mode      S>C 0\nRPRT 0
C>S q                   S>C RPRT 0
```

## Drei Befunde, alle am Draht gefunden

**1. `M CW 500` kam nie an — `targetable_vfo` war zu eng.**
Die Frequenz ließ sich setzen, der Modus nicht, und zwar ohne jede
Fehlermeldung. Im Mitschnitt fehlte der Befehl ganz: der Client hatte
ihn selbst verworfen. `targetable_vfo` ist eine Maske; mit `0x1`
(nur RIG_TARGETABLE_FREQ) schickt ein Client keinen Modusbefehl. Jetzt
`0x3` (FREQ | MODE) — bei uns richtig, weil jeder Befehl denselben
Slice hinter VFOA adressiert.

**2. Die VFO-Maske in den Frequenzbereichen entscheidet mit.**
Hamlib sucht den Bereich, gegen den es einen Setzbefehl prüft, über die
VFO-Maske der Bereichszeile. Passt sie nicht zur VFO des Clients, gibt
es keinen Bereich und der Befehl wird lokal abgelehnt. Übernommen ist
die Maske, die Hamlibs eigenes Dummy veröffentlicht (`0x17e00007`).

**3. Lange Getter brauchen Wert **und** Status — sonst 20 Sekunden
Stillstand.** `\get_lock_mode` hatten wir mit `RPRT -11` beantwortet.
Der Client wartete daraufhin auf einen Wert, lief in eine
20-Sekunden-Zeitüberschreitung (`rig_set_mode: elapsed=20020ms`) und
schickte den nächsten Befehl erst danach — weshalb Befund 1 zunächst
wie „Modus wird ignoriert" aussah. Das Dummy antwortet `0\nRPRT 0`.
Jetzt lesen und setzen wir damit unsere Slice-Sperre.

## Lauf nach den Behebungen

```
F 14250000   →  RPRT 0
f            →  Frequency: 14250000
M CW 500     →  RPRT 0
m            →  Mode: CW / Passband: 500
M LSB 0      →  RPRT 0          (Breite 0 = Durchlass behalten)
m            →  Mode: LSB / Passband: 500
T 1          →  Invalid parameter   (Sendetor zu — so gewollt)
t            →  PTT: 0
```
Ganzer Durchlauf: 4 Sekunden (vorher 20 s Hänger).

Mit `RigctldAllowPtt=True`, sonst unverändert:

```
T 1  →  RPRT 0      t  →  PTT: 1
T 0  →  RPRT 0      t  →  PTT: 0
```

Und im Protokoll der App, mit geschlossenem Tor:

```
WRN: RigctldServer: refused PTT — transmit over rigctld is turned off
```

## Richtigstellung vom selben Tag (CAT-Lauf)

Der Lauf oben liest sich so, als seien Frequenz und Modus gesetzt
worden. Für die **Frequenz** stimmt das im unverbundenen Zustand
**nicht**: `f` kam aus **hamlibs eigenem Frequenzcache**. Gegenprobe
über die CAT-Brücke, gleiche App, gleicher Moment: `F 18100000` über
rigctld, `f` antwortet 18100000 — `FA;` über CAT antwortet
`FA00000000000;`. Ohne Funkgerät gibt es keinen Slice, und das Setzen
ging ins Leere.

Behoben: die rigctld-Brücke liest nach jedem Setzen zurück und
antwortet `RPRT -1`, wenn es nicht gegriffen hat (Prüffall
`a_set_that_cannot_take_is_refused`). Der Modus war davon nicht
betroffen — dort wurde ohne Cache gefragt, und der Prüfstand gegen ein
echtes RadioModel **mit** Slice zeigt, dass beides wirkt.

## Das Sendetor

- Standard: **zu**. Lesen und Abstimmen geht, Tasten nicht.
- `stop()` schließt es wieder; ein Neustart startet geschlossen. Der
  Prüfstand pinnt das (`stopping_the_server_closes_the_gate`).
- Entkeyen (`T 0`) ist **immer** erlaubt, auch bei geschlossenem Tor —
  sonst ließe ein Zuklappen mitten im Senden das Gerät getastet zurück.
- Ist das Tor offen, nimmt die Anforderung denselben Weg wie jede andere
  Sendequelle (MOX), mit dem Bandplan-Wächter davor. Dieser Weg ist
  ohne Funkgerät nicht zu Ende geprüft.

## Was offen ist

- Am echten Funkgerät ist nichts davon gefahren. Offen sind vor allem:
  greift `T 1` mit offenem Tor bis an den Sender durch (und hält der
  Bandplan-Wächter dort), und stimmt die gemeldete Durchlassbreite mit
  dem, was der Empfänger wirklich macht.
- Noch nicht bedient: `l`/`L` außer KEYSPD, `\dump_caps`,
  Antennenwahl, RIT/XIT, Split (bei uns absichtlich nicht vorhanden —
  die unterstützte Client-Einstellung ist „Fake It").
- Der Kenwood-CAT-Befehlssatz über TCP und serielle Schnittstelle (das,
  was Thetis „CAT" nennt) fehlt weiter. rigctld deckt N1MM, DXLog und
  WSJT-X ab; CAT ist der nächste Schritt derselben Phase.
- Noch nicht gegen N1MM/DXLog selbst geprüft, nur gegen `rigctl`.
