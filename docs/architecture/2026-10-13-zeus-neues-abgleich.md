# Zeus 2.0.46 … 2.0.50 gegen Longpath

Stand 2026-10-13. Drei Zeus-Mitteilungen, Punkt für Punkt gegen den Baum
geprüft — nicht geschätzt. Jede Zeile „fehlt" ist ein `grep` über `src/`,
nicht über `src/core` allein (die Falle vom 2026-10-11, siehe
`feedback-verneinung-gilt-nur-am-ort`).

## 1. Sofort sinnvoll — baut auf Fertiges auf

| Zeus | Longpath | Aufwand |
| --- | --- | --- |
| ✔ **GEBAUT 2026-10-13** (`e0158ab2`) — **CAT antwortet auf die Thetis-ZZ-Befehle**; N1MM+ (TCP, Flex5000) und DXLab Commander (Gerätetyp Thetis) hängen sich damit direkt an | **fehlt.** Der CAT-Server von gestern spricht die 16 Kenwood-Befehle, keinen einzigen ZZ. Thetis hat ~10.500 Zeilen `CATCommands.cs`, davon ist der ZZ-Satz der größere Teil | mittel, in Gängen |
| ✔ **GEBAUT 2026-10-13** (`ac95cd54`) — **Thetis-TX-Profile einlesen** (XML-Export oder ganze Thetis-Datenbank, selbst erkannt; je Profil ein Profil, nichts wird überschrieben; Mic-Gain, TX-Filter, Leveler, ALC, Kompressor, CESSB, CFC, Phasendreher, DEXP, FM-Sendeton, AM-Träger; EQ auf den 10-Band-EQ umgerechnet; Hinweis je angepassten Wert) | **fehlt.** `MicProfileManager` hat 22 Werksprofile und kein Einlesen. Longpath IST ein Thetis-Port: die Felder stehen 1:1 gegenüber, der AM-Träger seit heute Nacht, der TX-EQ seit 3M | mittel |

Beide liegen auf Arbeiten, die diese Woche fertig geworden sind — darum
zuerst.

## 2. Klein und lohnend

* ~~**Zahlenfelder werden von selbst wirksam**~~ — **KEIN Baubedarf,
  nachgesehen am 2026-10-13.** Mein erster Eintrag hier war zu schnell:
  ich hatte nach einem Nachlauf (`QTimer` + `editingFinished`) gesucht,
  keinen gefunden und auf „fehlt" geschlossen. Nachgesehen, welche
  Felder Zeus nennt, und wie sie bei uns aussehen:

  | Zeus' Feld | bei uns |
  | --- | --- |
  | TX-Filterkanten | `QSpinBox` auf `valueChanged` (`TxApplet.cpp:1276`) |
  | Phasendreher | `QSpinBox` auf `valueChanged` (`DspSetupPages.cpp:2248`) |
  | CFC Pre-Comp | `QSpinBox`, gleiche Form |
  | FM-Relaisablage | `QSpinBox` in kHz auf `valueChanged` (`VfoModeContainers.cpp:273`) **und** ein Schieber im Phone/CW-Feld (`PhoneCwApplet.cpp:1054`) |

  Unsere Felder wirken also **bei jedem Tastendruck**, nicht erst beim
  Verlassen — Zeus' Problem gibt es hier nicht. Es ist nur anders
  gelöst: beim Tippen von „1500" gehen 1, 15, 150 und 1500 durch die
  DSP. Bei einer Filterkante oder einer Phasendreherfrequenz ist das
  harmlos.

  **Zwei eigene Fehlschlüsse in diesem einen Eintrag**, beide beim
  Nachsehen aufgefallen:

  1. Ich hatte nach einem Nachlauf gesucht, keinen gefunden und auf
     „fehlt" geschlossen. **Ein leeres `grep` nach einer LÖSUNG heißt
     nicht, dass das PROBLEM da ist** —
     [[feedback-abwesenheit-ist-keine-aussage]] gilt auch umgekehrt.
  2. Dann schrieb ich, die FM-Relaisablage habe gar keine Bedienung —
     **und hatte das falsche Symbol gegrept.** `shiftOffsetHz` ist die
     Platzierung einer Scheibe im Abtaststrom (DDC), nicht die
     Relaisablage; die heißt `fmOffsetHz` und hat sowohl ein Zahlenfeld
     in den VFO-Behältern als auch einen Schieber im Phone/CW-Feld.
     [[feedback-verneinung-gilt-nur-am-ort]], wieder.
* **Squelch AN/AUS je Band gemerkt** (und je Transverter). Bei uns hängt
  die Schwelle am Slice (`m_amsqThresh`, `m_fmsqThresh`), der Schalter an
  nichts Bandbezogenem. „SQL für 2 m FM" folgt also auf 20 m mit.

  **Nachgesehen am 2026-10-13: größer als es klingt.** Longpath hat
  keinen allgemeinen Weg, eine Scheiben-Einstellung je Band zu merken —
  bandbezogen sind nur das Panadapter-Gitter
  (`PanadapterModel::m_perBandGrid`) und die Anlege-Vorgaben
  (`BandDefaults`). Das zu bauen heißt, einen neuen Speicherweg an
  `SliceModel` zu hängen, und der ist die Stelle, an der auch
  Band/Modus/Filter beim Wechsel wiederhergestellt werden. Also nicht
  „eine Zeile", sondern ein eigener Gang — und einer, bei dem ein Fehler
  den Bandwechsel trifft.

  **Dazu kommt ein echter Fund**, der beim ZZ-Satz nebenbei anfiel:
  `RadioModel::setSqlEnable()` — der Weg, den auch der TCI-Server nimmt
  — greift ausschließlich auf `ssqlEnabled`. **In FM schaltet er nichts**,
  denn die FM-Sperre hängt an `fmsqEnabled`. Longpath hat drei Schalter
  (ssql für SSB/CW/Digi, amsq für AM/SAM, fmsq für FM), CAT und TCI
  haben einen. Für CAT ist das jetzt modusrichtig verdrahtet
  (`MainWindow.cpp`, eigene Helfer, Prüffall in `tst_cat_verdrahtung`);
  an `setSqlEnable()` selbst ist **nichts geändert**, weil es TCI
  mitbetrifft.
* **Transverterbänder merken den letzten Modus** — gleiche Ecke.

## 3. Schon vorhanden — nicht nachbauen

* **AGC je Empfänger.** Zeus hat das gerade getrennt; bei uns liegt
  `agcMode` seit immer am `SliceModel` (`SliceModel.h:179`), also je
  Empfänger.
* **Panadapter-Füllung abschaltbar** — `SpectrumWidget::setPanFillEnabled`
  (:2132), dazu `setFillAlpha`, `setFillColor`.
* **Spitzenhalten am Instrument** — `PeakHold` mit Abfallrate
  (`MeterItem.h:504-508`), Nadel und Balken getrennt.
* **Beschädigte Einstellungsdatei wird beiseitegelegt** — der ganze
  Wiederherstellungsweg steht in `AppSettings.cpp:498-642` (#241/#244).

## 4. Gemessen — ✔ erledigt am 2026-10-13

* **Multi-RX springt lauter: ja, Faktor genau 2,000.** Gemessen, nicht
  gelesen (`tests/tst_multi_rx_pegel.cpp`):

  ```
  ein Empfänger   0,500 hinein -> 0,500 hinaus
  zwei Empfänger  0,500 je     -> 1,000 hinaus    == Faktor 2,000
  drei, einer stumm            -> 1,000 hinaus    == stumme zählen nicht
  ```

  Die zweite Hälfte von Zeus' Meldung (stumme zählen nicht mit) stimmt
  bei uns schon. **Nicht geändert**, und das ist eine Entscheidung:
  WDSPs `aamix.c` summiert genauso glatt — Zeus weicht hier von der
  Vorlage ab, nicht wir — und es ist eine Frage des Hörpegels, die
  Martin gehört ([[feedback-messung-schlaegt-nicht-das-geraet]]). Eine
  Teilung macht den einen Empfänger nicht leiser, aber jeden weiteren.
  Der Prüffall nagelt das heutige Verhalten fest, damit eine Änderung
  später eine Entscheidung ist und kein Versehen.

## 5. Wert, aber groß

* **Capture RX/TX**: 1–10 s Ton einfrieren und in einen großen
  Analysator geben (zoomen, messen, als Bild ausgeben, eigenes Fenster,
  übersteht einen Neuladen). Fehlt ganz.
* **MIDI und Stream Deck** — fehlt ganz.
* **Empfängerkopf „nur Frequenz"** (Stil 2, 0 % Deckkraft, am Rand
  ziehbar) und **Deckkraftregler an der VFO-Karte**, die bis zur
  Frequenz herunterblendet, bis man darüberfährt. Beides Gestaltung.
* **Endstufen-Watt im S-Meter** hinter der Vorwärtsleistung des Geräts.
  Bei uns sind SPE/KPA500/ACOM angebunden, aber kein gemessener Wert im
  Instrument.

## 6. Passt nicht oder liegt bewusst draußen

* **CW+ und der Iambic-Mithörton** — CW ist zurückgestellt
  (`contestprogramm-winkeyer-deferred`), nicht von selbst wieder
  vorschlagen.
* **Mobile-Ansicht, Mesh Messenger, Community-Katalog, ZeusChat,
  Zeus-Link-Punkte, Chat-Zugang** — kein Gegenstück in Longpath
  (Longpath ist keine Webanwendung und hat keinen Erweiterungskatalog).
* **Home Assistant, Wetterfeld, HamClock, RTL-SDR, VST, Tuner Genius
  XL** — das sind neue Fremdanbindungen, nicht „Bestehendes ausbauen".
  `TunerGeniusXL` steht bei uns nur als Modellname in der
  SmartSDR-Horchstelle (`SmartSdrApiListener.h:120`).
* **ASIO-Resync** — ASIO ist Windows; unser Tonweg ist CoreAudio.
* **Zweiton wartet auf die TX-Verzögerung der Endstufe**: Zeus gleicht
  den Zweiton an MOX und Tune an. Bei uns gibt es **gar keine
  PA-TX-Verzögerung** (`grep txDelay` findet nur eine Kiwi-Fahne).
  `TwoToneController` kennt nur Nachlaufzeiten beim Loslassen
  (`setSettleDelaysMs`). Das ist also kein Angleichen, sondern ein neuer
  Begriff — gehört zu den Endstufen, nicht hierher.
