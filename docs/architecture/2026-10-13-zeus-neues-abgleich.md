# Zeus 2.0.46 … 2.0.50 gegen Longpath

Stand 2026-10-13. Drei Zeus-Mitteilungen, Punkt für Punkt gegen den Baum
geprüft — nicht geschätzt. Jede Zeile „fehlt" ist ein `grep` über `src/`,
nicht über `src/core` allein (die Falle vom 2026-10-11, siehe
`feedback-verneinung-gilt-nur-am-ort`).

## 1. Sofort sinnvoll — baut auf Fertiges auf

| Zeus | Longpath | Aufwand |
| --- | --- | --- |
| **CAT antwortet auf die Thetis-ZZ-Befehle**; N1MM+ (TCP, Flex5000) und DXLab Commander (Gerätetyp Thetis) hängen sich damit direkt an | **fehlt.** Der CAT-Server von gestern spricht die 16 Kenwood-Befehle, keinen einzigen ZZ. Thetis hat ~10.500 Zeilen `CATCommands.cs`, davon ist der ZZ-Satz der größere Teil | mittel, in Gängen |
| **Thetis-TX-Profile einlesen** (XML-Export oder ganze Thetis-Datenbank, selbst erkannt; je Profil ein Profil, nichts wird überschrieben; Mic-Gain, TX-Filter, Leveler, ALC, Kompressor, CESSB, CFC, Phasendreher, DEXP, FM-Sendeton, AM-Träger; EQ auf den 10-Band-EQ umgerechnet; Hinweis je angepassten Wert) | **fehlt.** `MicProfileManager` hat 22 Werksprofile und kein Einlesen. Longpath IST ein Thetis-Port: die Felder stehen 1:1 gegenüber, der AM-Träger seit heute Nacht, der TX-EQ seit 3M | mittel |

Beide liegen auf Arbeiten, die diese Woche fertig geworden sind — darum
zuerst.

## 2. Klein und lohnend

* **Zahlenfelder werden von selbst wirksam**, kurz nachdem man zu tippen
  aufhört (Zeus: TX-Filterkanten, CFC Pre-Comp, Phasendreher,
  FM-Relaisablage). Bei uns gibt es nirgends einen solchen Nachlauf
  (`grep` auf `QTimer` + `editingFinished`/`debounce` leer) — überall
  muss Eingabe oder Fokuswechsel kommen. Eine Hilfsklasse, dann je Feld
  eine Zeile.
* **Squelch AN/AUS je Band gemerkt** (und je Transverter). Bei uns hängt
  die Schwelle am Slice (`m_amsqThresh`, `m_fmsqThresh`), der Schalter an
  nichts Bandbezogenem. „SQL für 2 m FM" folgt also auf 20 m mit.
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

## 4. Zu prüfen, könnte derselbe Fehler sein

* **Multi-RX springt lauter.** Zeus hat behoben, dass mitlaufende
  Empfänger ihre Pegel aufaddieren statt den Lautsprecherpegel zu teilen
  (stumme zählen nicht mit). Bei uns ist nichts in der Art zu finden
  (`sharedSpeakerLevel`/`normalizeAf` leer) — **also messen, nicht
  annehmen**: zwei Slices anlegen und den Ausgangspegel vergleichen.

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
