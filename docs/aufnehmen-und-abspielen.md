# Aufnehmen und abspielen

Longpath nimmt auf **drei** Arten auf. Sie landen im selben Ordner und
sehen dort fast gleich aus — darum zuerst die Tabelle, dann die
Handgriffe.

| | QSO-Mitschnitt | Tonaufnahme | I/Q-Aufnahme |
| --- | --- | --- | --- |
| Wo bedient | **QSO-Mitschnitt**-Applet | **Aufnahme**-Applet, Zeile *Audio* | **Aufnahme**-Applet, Zeile *I/Q* |
| Was drin ist | fertiger Ton, kurze Stücke | fertiger Ton, so lang wie man will | das rohe Signal **vor** der Demodulation |
| Abtastrate | 48 kHz | 48 kHz | die der Verbindung (z. B. 192 kHz) |
| Platz je Minute | wenige MB | wenige MB | **viel** — 192 kHz Stereo float sind rund 90 MB/min |
| Lässt sich abspielen | im eigenen Applet | nein (mit einem Tonspieler außerhalb) | **ja, in Longpath** |

Die eine Sache, die man wissen muss: **abspielen kann Longpath nur
I/Q-Aufnahmen.** Eine Tonaufnahme ist fertiger Ton — es gibt nichts
mehr zu demodulieren, kein Spektrum zu rechnen, keine Betriebsart zu
wechseln. Darum nimmt der Knopf *▶ Open…* sie nicht an und sagt es
auch (bis zum 2026-10-22 nahm er sie an und spielte Rauschen).

## Wo die Dateien liegen

```
~/Library/Preferences/Longpath/recordings/
```

Der Finder versteckt `~/Library`. So kommt man hin:

```bash
open ~/Library/Preferences/Longpath/recordings
```

(Unter Linux und Windows liegt er entsprechend im
Einstellungsordner. Läuft Longpath mit `LONGPATH_CONFIG_DIR`, liegt er
darin — maßgeblich ist `AppSettings::dataDir()`.)

Der Ordner wird **erst beim ersten Aufnehmen angelegt**. Ist er nicht
da, wurde auf diesem Rechner noch nie aufgenommen; das ist kein Fehler.

### Wie die Namen aussehen

```
20261022-031500Z_20m_USB.wav        ← Ton
20261022-031500Z_20m_USB.json       ← sein Beizettel
20261022-031500Z_20m_USB_iq.wav     ← I/Q
20261022-031500Z_20m_USB_iq.json    ← sein Beizettel
```

Zeitstempel in **UTC**, dann Band und Betriebsart. Das `_iq` ist nicht
Zierde: vorher hießen beide Sorten gleich, und wer Ton und I/Q in
derselben Sekunde startete, verlor die erste Aufnahme.

Der **Beizettel** neben jeder Aufnahme nennt Startzeit, Frequenz,
Betriebsart, Band, Abtastrate, Bittiefe, Länge und unter `source`, was
es ist (`off the air` für Ton, `raw I/Q, pre-demodulation` für I/Q).
Genau daran erkennt die Wiedergabe die Sorte. Wer eine Aufnahme von
Hand kopiert, nimmt den Beizettel mit — ohne ihn geht die Wiedergabe
zwar trotzdem, aber niemand weiß mehr, auf welcher Frequenz das war.

## Eine Aufnahme abspielen

1. Im **Aufnahme**-Applet in der Zeile *Playback* auf **▶ Open…**.
2. Der Wähler bietet `*_iq.wav` zuerst an. Ältere I/Q-Aufnahmen tragen
   die Kennung nicht — dafür ist der zweite Filter *All recordings* da.
3. Die Aufnahme läuft durch **dieselbe Kette wie ein echtes Gerät**:
   Wasserfall, Panadapter, WDSP, Ton. Betriebsart, Filter und Rauschminderung
   wirken dabei wie im Betrieb — man kann also im Nachhinein anders
   demodulieren, als man damals gehört hat.
4. **■ Stop** beendet sie. Am Dateiende hört sie von selbst auf.

Die Wiedergabe tastet **nie** den Sender: `PlaybackRadioConnection`
ist eine reine Empfangsverbindung, jeder Sendeweg darin ist ein
Leerlauf.

### Wenn nichts kommt

| Was zu sehen ist | Woran es liegt |
| --- | --- |
| „that is an audio recording …" | Tonaufnahme erwischt. Eine mit `_iq` nehmen. |
| Der Knopf ist grau | Kein Radio verbunden — die Wiedergabe hängt an derselben Maschine. |
| „no I/Q on this receiver" beim **Aufnehmen** | Die Scheibe kommt von einem fremden Gerät (Kiwi, SunSDR, Flex). Dort rechnet nicht Longpath; es kommen fertige Töne an, kein I/Q. |

## Nach Plan aufnehmen

Im **Aufnahme**-Applet auf **Plan…**:

* Startzeit in **UTC** — nicht Ortszeit. Eine Aufnahme über Mitternacht
  trüge sonst zweimal denselben Namen.
* Dauer in Minuten. **0 heißt offenes Ende**: sie läuft, bis man sie
  selbst beendet.
* Art: *Audio* oder *I/Q*.
* **Remove** löscht den **ausgewählten** Eintrag.

Die Hinweiszeile darunter sagt, ob der Plan scharf ist. Steht dort
„no radio is attached, so nothing will fire", ist er es nicht —
der Takt läuft erst, wenn ein Radio hängt.

Die Einträge **überleben den Neustart**. Was es bewusst **nicht** gibt:
Kalender und Wiederholungen. Beides kennt der Unterbau nicht, und eine
Fläche, die mehr verspricht als die Technik hält, ist schlimmer als
keine.

## Platz im Auge behalten

Eine I/Q-Aufnahme bei 192 kHz kostet rund **90 MB je Minute** (Stereo
float32), bei 384 kHz das Doppelte. Eine Stunde sind also gut 5 GB.
Nichts räumt sie von selbst weg; der Ordner wächst, bis man ihn
aufräumt.

Wer nur hören will, was gesagt wurde, nimmt **Ton** auf. I/Q braucht
man, wenn man die Aufnahme später **anders** demodulieren oder im
Spektrum nachsehen will — dafür ist sie da, und nur dafür lohnt der
Platz.
