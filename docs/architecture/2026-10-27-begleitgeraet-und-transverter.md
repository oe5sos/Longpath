# Begleitgerät und Transverter — Stufe 1

**Stand 2026-10-27.** Gebaut, geprüft, am Gerät noch nicht gefahren.

## Wofür

Der Betreiber baut eine 2-m-Conteststation. Ein klassisches Funkgerät
(TS-590S oder Elecraft K3, Entscheidung offen) arbeitet als ZF hinter
einem Transverter und **sendet**; Longpath hängt mit einem SDR am
ZF-Ausgang des Transverters und **zeigt das Band**.

Die Betriebsforderung, die alles bestimmt: *„sollte ich Probleme haben
und am Berg sein, kann ich jederzeit normal funken."* Longpath sitzt
deshalb **nie in der Betriebskette**. Rechner aus, SDR ab — der
Funkbetrieb läuft unverändert weiter. Darum hängt der Panadapter am
IF-Combiner des Transverters und nicht am Gerät.

Daraus folgen genau zwei Dinge, die Longpath fehlten:

1. **Wo steht das Gerät?** Longpath sprach rigctld bisher nur als
   Server (`RigctldServer`, #308 — fremde Programme steuern Longpath).
   Die Gegenrichtung fehlte.
2. **Was liegt auf der Antenne?** Das SDR sieht 28,3 MHz; auf der
   Antenne liegt 144,3 MHz. In Longpath gab es dafür nichts — „XVTR"
   ist dort nur eine Antennenbuchse (`BoardCapabilities::xvtrJackCount`).

## Was in dieser Stufe entstanden ist

| Datei | Was |
| --- | --- |
| `src/core/TransverterSetup.{h,cpp}` | Die Umrechnung ZF ↔ Antenne: ZF-**Fenster** plus Versatz, Vorgaben für die üblichen Kombinationen, Ablage in AppSettings |
| `src/core/HamlibDaemonProcess.{h,cpp}` | Der Rumpf, der bis heute in `RotctldProcess` saß — Programmsuche, Portwahl, Start, Abbruch, der Grund aus Hamlibs stderr |
| `src/core/RotctldProcess.{h,cpp}` | jetzt die rotctld-Ausprägung davon; Verhalten unverändert |
| `src/core/RigctldProcess.{h,cpp}` | die rigctld-Ausprägung, Port 4532 |
| `src/core/RigctldClient.{h,cpp}` | der CAT-Client: `f` / `F` / `m` / `t`, Parser als reine Funktionen |
| `src/gui/setup/CompanionRigPage.{h,cpp}` | Setup → CAT & Network → Companion Rig |
| `HamlibInstaller::supportedRigs()` | die Geräteliste aus `rigctl --list`, zur Laufzeit |

### Warum ein Fenster und kein Bandpaar

Das Contestprogramm löst dasselbe über ein Paar von Bandnamen
(„144" → „1296"), weil es UKW-Bänder kennt. Longpath kennt sie nicht:
seine Band-Aufzählung endet bei 6 m. Ein Bandpaar wäre hier eine
Erfindung, die nirgends anschließt. Ein ZF-**Fenster** plus Versatz ist
dagegen genau das, was ein Transverter tut — er mischt einen Bereich,
kein Band.

Der Fall, auf den es ankommt, ist der **dritte**: eine Frequenz
außerhalb des Fensters bleibt unberührt. Wer mit eingeschaltetem
Transverter auf 20 m hört, sieht 14 MHz und nicht 130 MHz. Eine
Rechnung, die immer addiert, wäre schlimmer als keine.

### Zwei Entscheidungen, die im Code stehen

**Longpath tastet das fremde Gerät nie.** rigctld kann `T 1`; dieser
Client kann es nicht, ohne Schalter. Ein fremdes Gerät an einer
Endstufe und einer Antenne wird nicht von einem Programm getastet, das
nur zuschaut. PTT wird **gelesen** (`t`), damit die Anzeige weiß, dass
gesendet wird. `tst_rigctld_client` schreibt jeden abgeschickten Befehl
mit und sieht die Liste durch — ein `T`, das sich einschleicht, fällt
im Prüfstand auf und nicht am Berg.

**Abstimmen geht.** `F <hz>` ist der eigentliche Nutzen eines
Panadapters: hineinklicken, und das Gerät geht dorthin. Eine Abstimmung
tastet nichts.

### Die Falle, die die Vorlage noch hat

Die Modusantwort von rigctld ist **zweizeilig**. Das Contestprogramm
(dessen `RigctldClient` dieselbe Form hat) wartet darauf unbedingt und
hängt bis zur Antwortwache, wenn rigctld einzeilig `RPRT -11` schickt —
was bei einem Gerät ohne Modusabfrage vorkommt. Vier Sekunden
Stillstand bei jeder Modusabfrage. Hier beendet eine RPRT-Zeile das
Warten sofort; `einzeiligeAntwortAufDieModusabfrageHaengtNicht` belegt
es.

### Modellnummern nie einprogrammieren

Die Geräteliste kommt zur Laufzeit aus `rigctl --list`. Nummern wandern
zwischen Hamlib-Fassungen; beim Contestprogramm war 1042 als FT-710
notiert und ist der FTDX-10. Die Vorauswahl der üblichen ZF-Geräte
geschieht über den **Namen**, nie über die Nummer.

## Geprüft

| Prüfstand | Fälle |
| --- | --- |
| `tst_transverter_versatz` | 12 — Umrechnung hin und zurück, Grenzen, unbrauchbare Einstellungen, negativer Versatz, Beschreibung, Vorgaben, Ablage |
| `tst_rigctld_client` | 15 — die vier Parser, Rundlauf gegen ein gefälschtes rigctld, der Hänger-Fall, Abstimmen, **nie ein Tastbefehl**, Trennen |
| `tst_begleitgeraet_seite` | 8 — Felder, MHz-Rundung, Vorgaben, Ablage über die Seite hinaus, „Custom", Geräteliste, Testknopf, Bild |

Gegenprobe gefahren: ohne die Fensterprüfung in `rfFrequencyHz()` fällt
`tst_transverter_versatz` um. Die vier Rotor-Prüfstände
(`tst_rotctld_process`, `tst_rotctld_client`, `tst_hamlib_installer`,
`tst_rotor_ohne_rotctld`) waren vor und nach dem Herausziehen des
Daemon-Rumpfes grün.

Ein Fehler kam aus dem eigenen Prüfstand: `RigctldClient` hatte
`127.0.0.1` als Vorgabeadresse, womit „keine Adresse gesetzt" kein
Zustand mehr war und die Klasse sich ungefragt irgendwohin verbunden
hätte. Die Vorgabe gehört in die Einstellungsseite, nicht in die
Klasse.

## Was ausdrücklich fehlt

- **Die Dauerverbindung.** Eine Setup-Seite wird bei jedem Öffnen neu
  gebaut; was den Panadapter nachführt, muss länger leben und gehört zu
  `RadioModel` / `MainWindow`. Die Seite hat dafür einen Knopf „Test",
  der für die Dauer des Hinsehens verbindet.
- **Die Marke im Panadapter** an der CAT-Frequenz, und der Klick, der
  das Gerät dorthin stimmt.
- **Der Versatz in Anzeige, Bandplan und Log.** Die Rechnung steht und
  ist geprüft; sie wird bisher nur auf der Einstellseite angewandt.
- **Alles am echten Gerät.** Baudrate (TS-590S Menü 62, TS-590SG Menü
  68), PTT-Quelle, Anschlussname. Das ist der Teil, den kein Prüfstand
  vorwegnimmt.

## Hardware, Stand der Planung

Der Panadapter-Zweig kommt aus dem **IF-Combiner des Transverters**
(HA1YA MV-2, auf Wunsch eingebaut), nicht aus dem Gerät. Damit ist er
abziehbar, und die Frage S gegen SG gegen K3 entscheidet sich **nicht**
am Abgriff. Die DRV-Buchse des TS-590 liefert 0 dBm für den Transverter
— genau im Eingangsfenster des MV-2 (−20…+37 dBm).
