# Der Durchgang mit dem 50-Ohm-Abschluss

Alles, was an der SunSDR2 QRP noch offen ist, hängt an **einer** Sache:
einem Abschluss am Ausgang. Dieses Blatt macht daraus einen Durchgang
statt sieben einzelner Entdeckungen.

**Keine Antenne.** Ein Abschluss, keine Antenne — es wird mit Absicht
Leistung erzeugt, und die soll in einen Widerstand gehen, nicht in die
Luft und nicht in eine offene Buchse.

Stand beim Schreiben (2026-10-04): der **Empfang ist fertig**. Was hier
steht, ist das Senden plus die eine Empfangs-Bestätigung, die ohne
Sendezustand nicht geht.

---

## Vorher, ohne zu senden

| Schritt | Was prüfen | Abbruchgrund |
| --- | --- | --- |
| 1 | Abschluss **fest** angeschraubt, nicht nur aufgesteckt | sitzt er nicht, nicht weiter |
| 2 | Longpath verbunden, Empfang läuft, Wasserfall lebt | kein Strom → erst das klären |
| 3 | Im Log: `Folgenummern sauber`, Verlust unter 0,1 % | hoher Verlust → Netz klären, nicht senden |
| 4 | **Leistung ganz herunter**, bevor irgendetwas getastet wird | — |

Schritt 4 ist nicht formal: `setTxDrive` ist bei diesem Treiber ein
**leerer Rumpf**. Longpath kann die Leistung derzeit **nicht stellen**.
Was das Gerät beim Tasten abgibt, ist das, was zuletzt in ExpertSDR2
eingestellt war. Also dort vorher kleinstellen.

---

## Schritt A — Mikrofon-PTT bestätigen (Empfang, braucht aber Sendezustand)

Der einzige Empfangspunkt, der noch offen ist. Gebaut ist er
(`1e4eb060`): Longpath liest den Zustand aus dem **Stromkopf** —
Opcode `0xFD` heißt „Gerät sendet", `0xFE` heißt Empfang. Kein
Protokollwissen nötig, deshalb konnte er ohne Mitschnitt entstehen.

1. Mikrofontaste am Gerät **kurz** drücken und loslassen.
2. Im Log erwarten:

       SunSdr: PTT vom Geraet: gedrueckt (aus dem Stromkopf, Opcode 0xfd)
       SunSdr: PTT vom Geraet: losgelassen (aus dem Stromkopf, Opcode 0xfe)

3. **Genau zwei** Zeilen je Tastendruck, nicht zweihundert — die
   Flankenerkennung muss greifen (240 Pakete je Sekunde dürfen nicht 240
   Meldungen ergeben).

Kommt nichts: der Zustand steckt dann doch nicht im Stromkopf, und es
braucht den Mitschnitt. Kommt eine Flut: die Flankenerkennung greift
nicht, das ist ein Fehler in `pruefeMikrofonPtt`.

---

## Schritt B — die Opcode-Nummern bestätigen, **einzeln**

Hier liegt die eigentliche Arbeit, und hier ist auch die Falle. Am
2026-10-03 hat sich am Gerät gezeigt, dass die QRP **andere** Opcodes
benutzt als die DX/PRO, aus der alle unbestätigten Zahlen stammen:

| Befehl | QRP (gemessen) | DX (ArtemisSDR) |
| --- | --- | --- |
| Vorverstärker | `0x04` | `0x05` |
| DDC-Frequenz | `0x07` | `0x08` |
| VFO-Frequenz | `0x08` | `0x09` |
| erstes Byte des Rahmens | `0x03` | `0x32` |

**Nachtrag 2026-10-05 — drei davon sind keine Vermutung mehr.** Im
Mitschnitt `expert-96k.pcap` schickt ExpertSDR2 bei **jedem** Verbinden
`0x06` (MOX/PTT), `0x15` (Antennenwahl) und `0x17` (Drive) an die QRP,
jeweils mit Wert `00000000`, und das Gerät **quittiert jeden davon**.
Damit steht fest: die Opcodes existieren auf der QRP, und der Wert 0 ist
nachweislich harmlos. Offen bleibt, was ein Wert ≠ 0 bewirkt — und genau
dafür ist dieser Durchgang da.

`0x24` (PA freigeben) kommt im ganzen Verbindungsablauf **nicht** vor;
diese Nummer bleibt reiner Verdacht aus der DX-Quelle.

Für die übrigen gilt weiter: jede Zahl, die wir für das Senden benutzen,
stammt aus derselben Quelle wie die rechte Spalte, ist also **Verdacht,
nicht Fakt**:

| Zweck | Opcode | Stand 2026-10-05 | Gebaut? |
| --- | --- | --- | --- |
| MOX / PTT | `0x06` | **an der QRP quittiert** (Wert 0) | Rahmenbauer fertig, nicht verdrahtet |
| Leistung | `0x17` | **an der QRP quittiert** (Wert 0) | Rahmenbauer fertig, `setTxDrive` leer |
| PA freigeben | `0x24` | **nie gesehen** — reiner Verdacht | Rahmenbauer fertig, nicht verdrahtet |
| Antennenwahl | `0x15` | **an der QRP quittiert** (Wert 0) | verdrahtet, **stumm** (`LONGPATH_SUNSDR_ANTENNE=1`) |

**Vorgehen: einer nach dem anderen, und nach jedem nachsehen.** Das
Gerät quittiert jeden angenommenen Steuerrahmen binnen 15–50 ms
(2026-10-03 gemessen). Diese Quittung ist der Prüfstein:

- **quittiert** → der Opcode existiert und wurde angenommen
- **nicht quittiert** → die Zahl ist falsch; nicht nachlegen, sondern
  aufhören und den Mitschnitt machen

Im Log steht beides von selbst (`nach N ms quittiert` bzw. beim Trennen
`noch unquittiert: 0x..`).

**Die Antennenwahl zuerst**, weil sie als einzige nichts erzeugt: sie
schaltet nur ein Relais.

~~Erst wenn `0x15` quittiert wird, hat die Vermutung „die QRP teilt die
Opcodes der DX" überhaupt Grundlage.~~ — **erledigt am 2026-10-05**, ohne
Abschluss und ohne Senden: `0x15` wird quittiert, `0x06` und `0x17` auch.
Dieser Durchgang fängt damit nicht mehr bei der Frage an, ob die Nummern
stimmen, sondern bei der, was ein Wert ≠ 0 bewirkt.

---

## Schritt C — erstmals tasten

Erst wenn A und B durch sind.

1. Leistung in ExpertSDR2 auf den kleinsten Wert, dann ExpertSDR2
   schließen.
2. Kurz tasten — **eine Sekunde**, nicht mehr.
3. Danach sofort: Gerät handwarm? Abschluss handwarm? Dann ist Leistung
   geflossen, wo sie soll.
4. Im Log: Stromkopf auf `0xFD`, und nach dem Loslassen zurück auf
   `0xFE`.

**Es gibt keine Rückmeldung über Leistung oder Stehwelle.** Am
2026-10-03 wurde gemessen, dass die QRP im Empfang **keine Messwerte
herausgibt** — alle Abfrageantworten waren über zehn Tage bitgleich. Ob
sie im Sendezustand welche liefert, ist unbekannt. Bis das geklärt ist,
ersetzt der Handrücken das Instrument, und deshalb bleibt es bei einer
Sekunde.

---

## Was danach feststeht

- Mikrofon-PTT bestätigt → der Empfang ist **abgeschlossen**, ohne
  Vorbehalt.
- Opcodes bestätigt oder widerlegt → entweder kann das Senden gebaut
  werden, oder es braucht zuerst einen Mitschnitt mit ExpertSDR2 beim
  Senden. Beides ist ein klares Ergebnis; nur das Raten dazwischen muss
  aufhören.

Siehe auch `docs/architecture/2026-10-02-sunsdr-paritaet.md` (Lückenliste,
Abschnitte 3a und 4) und `docs/development/sunsdr-mitschnitt-anleitung.md`
(wenn der Mitschnitt doch gebraucht wird).
