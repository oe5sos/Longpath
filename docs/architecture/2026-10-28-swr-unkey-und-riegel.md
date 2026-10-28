# SWR-Schutz: Freigabe bei jedem Unkey, Riegel nach offener Antenne

**2026-10-28** — gegen Thetis `v2.10.3.15-5-g852bf0e` (`console.cs`
`PollPAPWR` und `UIMOXChangedFalse`).

Der Betreiber hat diese beiden Punkte am 28.10. aus der
Upstream-Durchsicht ausgewählt. Beide waren in der Durchsicht nur als
„Thetis-Treue" notiert. Beim Lesen der Quelle wurde daraus etwas
anderes: einer der beiden ist eine Lücke, nicht eine Abweichung.

---

## 1. Die offene Antenne wurde erkannt und niemand las es

`SwrProtectionController::ingest()` hat die Heuristik aus Thetis
vollständig: mehr als 10 W vorwärts, weniger als 1 W Differenz zwischen
vorwärts und rückwärts, nicht am Tunen → SWR 50, Ansteuerung auf ein
Prozent, `openAntennaDetected()` wahr, Signal
`openAntennaDetectedChanged(true)`.

Thetis macht an derselben Stelle drei Dinge
(`console.cs:26041-26055 [v2.10.3.15]`):

```csharp
swr = 50.0f;
NetworkIO.SWRProtect = 0.01f;
chkMOX.Checked = false;
... checkAntennaWarning(); ...
goto end;
```

Die mittlere Zeile **tastet ab**. Wir hatten sie nicht. Ein `grep` über
`src/` nach `openAntennaDetected` fand den Namen ausschließlich im
Controller selbst und in seinem eigenen Prüfstand — kein einziger Leser
im ganzen Programm. Es wurde also auf ein Prozent zurückgefahren und
weitergesendet, in eine Leitung, die fast die volle Leistung
zurückmeldet.

Jetzt liest `RadioModel::handlePaTelemetry` den Befund, lässt den
Schlüssel fallen und gibt Thetis' Warntext aus.

### Der Riegel

Thetis hört nach dem `chkMOX.Checked = false` auf. Das reicht dort
nicht, und bei uns noch weniger: `PollPTT`
(`console.cs:25470-25555 [v2.10.3.15]`) darf im nächsten Durchgang
gleich wieder tasten, wenn die Taste noch gedrückt ist. Bei uns meldet
`RadioConnection` das Mikrofon-PTT auf **jedem** Statusrahmen, 50 bis
100 Mal in der Sekunde (der Kommentar in
`MoxController::onMicPttFromRadio` sagt das ausdrücklich). Aus „Taste
halten" würde also: tasten, auslösen, abtasten, tasten — Dutzende Male
pro Sekunde in eine offene Leitung.

Darum ein Riegel, `MoxController::latchKeyBlock()`. Er sitzt bei der
Bandplan-Prüfung, weil das das eine Tor ist, durch das jede Tastquelle
kommt: Mikrofon-PTT, VOX, CAT, TCI, der MOX-Knopf, TUNE und der
Zweitonlauf kommen alle als `setMox(true)` an.

Das Lösen ist die zarte Stelle. Es hängt an `setMox(false)` — **über**
dem Idempotenz-Wächter, damit es auch greift, wenn MOX längst aus ist
(wir haben es ja gerade fallen lassen). Das funktioniert, weil eine
**gehaltene** Taste nur `setMox(true)` erzeugt: `onMicPttFromRadio(false)`
startet den Nachlaufzeitgeber nur, wenn die Taste oben ist, und erst der
ruft `setMox(false)`. Also:

| Vorgang | Was ankommt | Riegel |
| --- | --- | --- |
| Taste gehalten | nur `setMox(true)`, 50–100/s | bleibt, jedes Mal abgewiesen |
| Taste losgelassen | Nachlauf → `setMox(false)` | gelöst |
| Nächster Druck | `setMox(true)` | tastet normal |
| MOX-Knopf, CAT, TCI aus | `setMox(false)` | gelöst |

Eine Folge davon, die im Code steht und hier auch: `RadioModel` setzt
den Riegel **nach** seinem eigenen `setMox(false)`, nicht davor — sonst
löst es, was es gerade gesetzt hat.

Die Warnung läuft über den Warnstreifen, den `MainWindow` schon für
`moxRejected` und die Bandplan-Absagen benutzt, mit Thetis' eigenen
Worten („High SWR condition detected. Please check your antenna
connection.") und 10 s statt 3 s — der Betreiber muss zur Antenne, nicht
bloß neu abstimmen. Thetis nimmt dort eine modale MessageBox; ein Kasten
mitten aus einer Schleife heraus ist nicht unsere Art, und für jede
andere verweigerte Tastung steht der Streifen schon da.

---

## 2. Die Freigabe bei jedem Unkey

Drei getrennte Befunde, alle am Lesen von `UIMOXChangedFalse`.

### 2a. Die Kante war eine Warteschlange

`m_swrProt.onMoxOff()` stand seit dem 2026-08-14 oben in
`RadioModel::onMoxHardwareFlipped` — der Fund damals war, dass es
*überhaupt niemand* rief und die erste Rückfahrt einer Sitzung darum
endgültig war. Das war richtig und bleibt.

Nur die Kante stimmte nicht: dieser Slot hängt an `hardwareFlipped`
über eine `Qt::QueuedConnection`. Die Freigabe lief also eine
Ereignisschleifenrunde später als das Unkey, das sie verlangt hat.

Die naheliegende Alternative — das Ende des Laufs — ist **schlechter**:
`setMox()` ruft `stopAllTimers()` bei jedem neuen Übergang, ein
Wiedertasten innerhalb der 30 ms Ausspülfrist löscht die Zeitgeber, und
die Freigabe liefe nie. Die Ansteuerung bliebe für die ganze nächste
Aussendung bei einem Prozent.

`moxChanging` wird synchron in `setMox` ausgelöst, nach dem
Idempotenz-Wächter (also ein echtes Unkey, keine Wiederholung) und vor
dem Festschreiben von MOX. Dort hängt sie jetzt.

**Was das ehrlich kostet:** das ist *früher* als Thetis, das nach dem
Hardware-Unkey zurücksetzt. Die Spanne ist begrenzt — die Zahl, die
zurückgestellt wird, liest `pumpAudioVolume`, und das läuft bei einer
Ansteuerungsänderung, nicht bei diesem Zurückstellen; das
zurückgefahrene Byte auf dem Draht bleibt also liegen, bis etwas die
Ansteuerung neu setzt. Was es aber heißt: während der 30 ms Ausspülfrist
ist der Schutz schon frei, und weil die Rechnung jetzt am MOX hängt,
wird ein hoher Stehwert in diesem Nachlauf nicht mehr bearbeitet.

### 2b. `onMoxOff()` löschte zwei Dinge zu viel

`UIMOXChangedFalse` setzt genau fünf Dinge zurück und **zwei
ausdrücklich nicht**:

| Thetis | Wir, vorher | Jetzt |
| --- | --- | --- |
| `SWRProtect = 1.0f` | ✓ | ✓ |
| `_wind_back_engaged = false` | ✓ | ✓ |
| `PowerFoldedBack = false` | über `highSwr` | ✓ |
| `HighSWRScale = 1.0` | — (kein Gegenstück) | — |
| `HighSWR = false` | ✓ | ✓ |
| `high_swr_count` — **bleibt** | gelöscht | bleibt |
| `alex_swr` — **bleibt** | auf 1,0 gesetzt | bleibt |

Der Trippzähler: Thetis trägt ihn über das Unkey. Loslassen bei drei
Trippversuchen und gleich wieder in denselben Fehlabschluss hinein
fährt dort bei der nächsten Probe zurück, statt vier neue zu gewähren.
Unser Löschen schenkte bei jedem Antippen ein frisches Zeitfenster.

Der gemessene Stehwert ist eine **Ablesung**, kein Riegel. Thetis
schreibt ihn nur an der `end:`-Marke einer getasteten Probe, er hält
also durch den Empfang. Unser Zurückstellen auf 1,00 wischte genau die
Zahl weg, derentwegen man loslässt.

### 2c. Die Rechnung lief, wenn nichts sendete

Thetis hält den ganzen Block unter `if (_mox)`
(`console.cs:25991 [v2.10.3.15]`). Bei uns fütterte
`handlePaTelemetry` den Controller mit jedem Rahmen, auch nach dem
Unkey — die tragen den letzten Kopplerwert plus Rauschen. Das wurde als
fwd≈0/rev≈0 gelesen, auf SWR 1,0 geklemmt, und lief durch den
Sauber-Zweig. Also: der Stehwert sprang auf 1,00, *und* Trippzähler und
Rückfahrt wurden von Proben gelöscht, während nichts sendete.

Jetzt gilt `isMox()` als Tor — nicht `state() == MoxState::Tx`, denn
Thetis' `_mox` ist das festgeschriebene Merker, das auch während seiner
`rf_delay` schon wahr ist, und `isMox()` ist unseres.

### 2d. Schutz abschalten hob die Rückfahrt auf

`ingest()` begann mit `if (!m_enabled) { … alles löschen … return; }`.
Das löschte auch den eingerasteten Rückfahrriegel. Thetis nicht:
`swrprotection` sperrt die drei Stellen, die einen Fehler **anlegen**
können (`26037`, `26118`, `26134`), aber die Zeile, die einen
eingerasteten Riegel **anwendet**, ist ungesperrt, und
`_wind_back_engaged` wird nur in `UIMOXChangedFalse` gelöscht.

Das ist kein Schönheitsfehler: mit dem alten Verhalten konnte man eine
echte Rückfahrt auf ein Prozent mitten im Senden aufheben, indem man
einen Haken wegnahm, und ging mit voller Ansteuerung in genau das
zurück, was sie ausgelöst hatte.

Ein Prüffall schrieb das alte Verhalten fest
(`setEnabled_false_clearsLatchedState`, aus einem
Codex-P2-Nachgang zu PR #139, mit der Begründung „phantom fault state").
Er ist umgedreht und begründet, nicht gelöscht — der Name bleibt, weil
er jetzt beschreibt, *was* der Aus-Zweig löscht und was nicht.

Eine Abweichung bleibt bewusst stehen: solange der Riegel hält und der
Schutz an ist, bleibt `highSwr` oben. Thetis hat dafür ein zweites
Anzeigemerker (`Display.PowerFoldedBack`); wir haben es nicht, und
`highSwr` ist das, was die Einblendung und die LED im TX-Feld
anzündet. Ginge es aus, wäre die Anzeige dunkel, während die
Ansteuerung bei einem Prozent steht.

---

## Was das NICHT ist

Die Durchsicht hatte sechs Punkte zu diesem Thema. Vier davon sind
bewusst **nicht** drin:

| Nicht gebaut | Warum |
| --- | --- |
| Die ANAN-8000D-Trippschwelle (`2 × PWR` auf jeder Probe) | Kein 8000D im Haus. Die Stelle rechnet das heute schon, nur an einer anderen Kante; sie ohne Gerät zu verschieben ist Raten |
| Die 8000D-Ausnahme bei der offenen Antenne | Dasselbe. Es bräuchte ein Modellargument für ein Gerät, das niemand hat |
| Das `alexpresent \|\| apollopresent`-Tor | Wir haben keine „Alex vorhanden"-Einstellung; Thetis hat sie standardmäßig an. Ein Tor ohne Schalter ist kein Tor |
| „Disable HF PA" (`hf_tr_relay` → `swr_pass`) | Eigener Punkt, eigene Einstellung |

Und es ist **keine** Änderung an Zahlen: Schwelle, Vorwärtsboden,
Trippschwelle, Rückfahrfaktor, Öffnungsschwellen — alle unverändert.

Der Hauptbefund der Durchsicht zum SWR-Schutz traf uns ohnehin nicht:
`d8ee2c8ad` sagt, der Rückfahrfaktor werde nirgends angewandt. Unserer
wird es seit je (`RadioModel.h:2033`, `computeWireDriveForTest`), und
der Upstream-Commit nennt unseren Commit `99b20c81` selbst namentlich.

---

## Belege

| Was | Wie geprüft |
| --- | --- |
| Trippzähler überlebt das Unkey | `unkeyBehaeltDenTrippzaehler` — drei Trippversuche, Unkey, eine weitere Probe löst aus |
| Stehwert überlebt das Unkey | `unkeyBehaeltDenGemessenenStehwert` |
| Der Riegel wird beim Unkey sehr wohl gelöst | `unkeyLoestDenRueckfahrriegel` |
| Schutz aus legt nichts Neues an | `schutzAusLegtKeinenNeuenRiegelAn`, `schutzAusMeldetKeineOffeneAntenne` |
| Schutz aus hebt Eingerastetes nicht auf | `schutzAusHebtEingerastetenRiegelNichtAuf` + der umgedrehte `setEnabled_false_clearsLatchedState` |
| Riegel weist ab und nennt den Grund | `riegelWeistDasTastenAbUndNenntDenGrund` |
| Gehaltene Taste, 25 Rahmen | `gehalteneTasteBleibtAbgewiesen` |
| Loslassen gibt frei | `loslassenGibtFreiUndDerNaechsteDruckTastet` |
| Freigabe läuft synchron im Unkey | `freigabeLaeuftSynchronImUnkey` — **ohne** `processEvents()` dazwischen, das ist der Punkt |
| Nachlauf-Rahmen löscht die Ablesung nicht | `derNachlaufRahmenLoeschtDieAblesungNicht` — über die neue Naht `handlePaTelemetryNotKeyedForTest`, die die TX-Vortäuschung weglässt |
| Offene Antenne tastet ab und riegelt zu | `offeneAntenneTastetAbUndRiegeltZu` — die Rohwerte werden im Lauf rücklaufend bestimmt statt geraten, über `radioStatus()`; so wandert der Fall mit, wenn sich die Skalierungstabellen ändern |

Am Gerät noch nicht gehört: die offene Antenne braucht einen Sender mit
abgezogener Leitung. Das steht aus.
