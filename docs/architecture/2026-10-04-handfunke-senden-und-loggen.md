# Handfunke: senden und loggen

**Auftrag (Betreiber, 2026-10-04):** *„ziel ist es, grundsätzlich auch zu
senden. loggen sollte vielleicht auch gehen."*

Dieses Blatt sagt, was dafür schon da ist, was fehlt, und — vor allem — welche
Entscheidungen dem Betreiber gehören. Es ist bewusst vor dem ersten Byte Code
geschrieben: beim Senden ist der Fehler, den man erst im Betrieb merkt, ein
Fehler auf der Antenne.

---

## Teil 1 — Senden

### Was schon da ist, und es ist mehr als erwartet

Der Sendeweg über TCI ist **vollständig gebaut und dreifach gesperrt**. Nichts
davon muss neu entstehen; es muss nur freigegeben und bedient werden.

| Schutz | Wo | Wirkung |
|---|---|---|
| `TciAllowRemoteTx`, ab Werk **False** | `TciServer.cpp:2405` | ohne diesen Schalter geht aus dem Netz gar nichts |
| Token | `TciServer.cpp:1961` | jede Netzverbindung muss sich anmelden |
| **Sperre 1 von 3** — Tasten | `TciServer.cpp:3454` | `trx:N,true` aus dem Netz wird nicht ausgeführt *und nicht bestätigt* |
| **Sperre 2 von 3** — Sendeton | `TciServer.cpp:3617` | TX-Audio aus dem Netz wird verworfen |
| **Sperre 3 von 3** — Abstimmträger | `TciServer.cpp:2903` | `tune:N,true` ist Senden, auch wenn es nicht so heißt |
| Sende-Mutex | `m_txAudioActiveClient` | nur **ein** Client darf Ton schieben |
| Sendezeit-Deckel | `TciServer.cpp:2414 ff.` | Dauersenden endet von selbst |
| Wachhund am Tastenden | `startKeyedWatchdog()` | verschwindet der tastende Client, wird entsperrt |

Der Kommentar an Sperre 2 sagt, warum es drei sein müssen und nicht eine:

> *Sperrte man nur das Tasten, könnte ein Client weiter TX-Ton einspeisen und
> über eine andere Quelle (VOX, lokales MOX) senden; sperrte man nur den Ton,
> könnte er tasten und einen Träger stehen lassen. Ein halb gesperrter Sendeweg
> ist kein gesperrter Sendeweg.*

Sperre 3 ist der Fund, den man beim Nachdenken über „senden" zuletzt macht:
der Abstimmträger heißt nicht so, ist es aber.

### Was fehlt — und es liegt alles auf der Seite

1. **Eine PTT-Bedienung.** Heute sind `SENDEN` und `TUNE` tote Schilder
   (`tx locked` / `tune locked`), und seit 2026-10-03 meldet die Zeile
   immerhin, wenn die *Station* sendet.
2. **Das Mikrofon des Telefons.** `getUserMedia` → Abtastratenwandlung →
   TCI-Binärrahmen mit `TxAudioStream` (Typ 2) und 64-Byte-Kopf. Der Server
   nimmt mu-law, Int16, Int24, Int32 und Float32 (`TciBinaryFrame`).
3. **Den Sende-Mutex holen.** Der Server gibt ihn dem Client, der tastet.
   Reihenfolge also: `trx:0,true` → Ton schieben → `trx:0,false`.

### Die Entscheidungen, die dem Betreiber gehören

| Frage | Warum sie nicht mir gehört |
|---|---|
| **Tastendruck halten oder umschalten?** | Ein Umschalter, der am Telefon in der Tasche hängenbleibt, ist ein Dauerträger. Ein Halte-Knopf ist sicherer und am Telefon unbequemer. |
| **Was bei Verbindungsabriss mitten im Senden?** | Der Wachhund entsperrt — aber nach welcher Zeit? Zu kurz zerhackt eine Durchsage, zu lang ist ein Träger auf der Antenne. |
| **Sendezeit-Deckel für den Netzweg?** | Heute gilt der allgemeine. Am Telefon wäre ein kürzerer sinnvoll. |
| **Welche Tonqualität?** | mu-law bei 12 kHz ist das, was herunterkommt. Für die Gegenrichtung ist das wenig; 16 Bit bei 12 kHz kostet die doppelte Datenmenge. |
| **Ob überhaupt — und mit welcher Antenne.** | Der erste Versuch gehört an eine Dummy-Last, nicht an den Beam. |

### Vorgeschlagene Reihenfolge

1. **Alles bauen, was nicht tasten kann.** Mikrofonaufnahme, Rahmenbau,
   Übertragung — gegen `attrappe.py`, die die Rahmen annimmt und nachzählt.
   Der Tastenweg bleibt dabei ausgebaut, nicht nur abgeschaltet.
2. **Tasten hinter zwei Schalter.** `TciAllowRemoteTx` am Pult **und** eine
   bewusste Freigabe auf der Seite selbst, die nicht über einen Nachladen
   hinweg bestehen bleibt.
3. **Erster Versuch an der Dummy-Last**, mit Blick auf das S-Meter eines
   zweiten Empfängers. Erst danach an eine Antenne.

**Was ich ohne ausdrückliche Freigabe nicht baue:** irgendeinen Pfad, der
`trx:N,true` oder `tune:N,true` aus der Seite heraus senden kann. Die Seite hat
das am 2026-09-30 einmal getan — ein Dauerträger mit voller Leistung, der nur
nicht „senden" hieß. Dass das nicht wieder passiert, ist mehr wert als jede
Bequemlichkeit.

---

## Teil 2 — Loggen

Hier gibt es **keine HF-Folgen**, und deshalb ist es der Teil, der zuerst
fertig werden kann.

### Was da ist

* `LogEntry` (`src/models/LogEntry.h`) — ein Kontakt, ADIF-3-Feldnamen.
* `AdifLog::read/write/merge` (`src/core/AdifLog.h`) — Datei lesen, schreiben,
  **und zusammenführen ohne Dubletten** (`isSameQso`).
* `LogbookWindow` hinter dem Rotor/Log-Feld; es besitzt die Datei und schreibt
  sie (`LogbookWindow.cpp:1419`).
* Das Logbuch loggt bereits Frequenz und Betriebsart vom Funkgerät.

### Der saubere Weg — und warum nicht der naheliegende

**Naheliegend wäre:** der Handfunke-Server (Python) hängt den QSO selbst an die
ADIF-Datei an. **Das ist falsch.** Dann schreiben zwei Programme in dieselbe
Datei, und genau diese Sorte Nebenläufigkeit hat am 2026-10-03 die Logzeilen
zerschrieben — mit dem Unterschied, dass es hier um Martins Kontakte ginge.

**Richtig ist:** ein Longpath-eigener TCI-Befehl, so wie `spectrum_span` einer
ist. Die Seite schickt den QSO, **Longpath** schreibt ihn durch den
vorhandenen Weg (`AdifLog::merge` → `write`). Eine Datei, ein Schreiber, keine
zweite Wahrheit.

Vorschlag für die Form, an `spectrum_span` angelehnt:

```
log_qso:<call>,<utc>,<freqHz>,<mode>,<rstS>,<rstE>[,<name>,<qth>,<comment>];
```

Antwort `log_qso_ok:<call>;` oder `log_qso_err:<grund>;` — damit die Seite
sagen kann, ob es geklappt hat, statt es zu behaupten.

### Sperren, die auch hier gelten

Loggen ist harmlos für die Antenne, aber nicht für die Daten. Also dieselbe
Linie wie beim Senden:

* **Token-Pflicht** — wie bei jedem Netzbefehl.
* **Kein Löschen, kein Ändern** über diesen Weg. Nur anhängen. Wer einen
  Kontakt korrigieren will, tut das am Pult.
* **Dubletten über `isSameQso`**, nicht über blindes Anhängen.
* Ein eigener Schalter (`TciAllowRemoteLog`), ab Werk **an** — anders als beim
  Senden, weil hier nichts abstrahlt.

### Was am Telefon dazukommt

Ein schmales Blatt: Rufzeichen, RST hin und her, Name/QTH optional. Frequenz,
Betriebsart und Zeit kommen aus dem laufenden Zustand — die hat die Seite
ohnehin schon.

---

## Reihenfolge, die ich vorschlage

1. **Loggen** zuerst, vollständig. Keine HF, sofort nützlich, und es übt den
   Weg „Seite → eigener TCI-Befehl → Longpath" ein, den das Senden später
   genauso braucht.
2. **Senden ohne Tasten** — Mikrofon, Rahmen, Übertragung gegen die Attrappe.
3. **Tasten**, erst nach ausdrücklicher Freigabe und erst an der Dummy-Last.
