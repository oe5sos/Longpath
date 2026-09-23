# QRP an der Bank, ohne Antenne — was sich damit klaeren laesst

Ohne Antenne faellt alles weg, was sendet. Das ist weniger Verlust, als
es klingt: der Sendeweg ist ohnehin nicht gebaut (`sendTxIq()` ist
leer), und die offenen Fragen, die uns heute wirklich aufhalten, sind
Empfangsfragen.

Drei Dinge, in dieser Reihenfolge. Zusammen gut eine Stunde, jedes
einzeln sinnvoll.

---

## 1. Stimmt das Messgeraet? (10 Minuten, hoechster Hebel)

Seit dem 2026-09-23 gibt es `tools/sunsdr_sim.py`, ein Messgeraet, das
eine QRP nachstellt. Alles, was wir kuenftig ohne Funkgeraet pruefen,
haengt daran, dass es sich wie das echte Geraet verhaelt. Die Bankstunde
ist die einzige Gelegenheit, das nachzumessen.

**Was zu tun ist:** die QRP einschalten und anschliessen, Longpath
starten, verbinden — und das Protokoll mitlaufen lassen:

```bash
cd ~/Longpath/NereusSDR && ./build.sh && ./run.sh
```

Interessant sind drei Zahlen, die das Messgeraet auch liefert:

| | Messgeraet | echte QRP |
|---|---|---|
| Pakete je Sekunde | 1563 | ? |
| Proben je Paket | 200 Paare | ? |
| Strom verstummt ohne Lebenszeichen nach | 8 s | ? |

Die ersten beiden liest man aus dem Longpath-Protokoll mit
(`SunSdr: …` Zeilen, Blockzahl je Sekunde). Die dritte ist die
wichtigste und die einzige, die etwas Geduld braucht: Longpath schickt
alle zwei Sekunden ein Lebenszeichen. Laeuft die Verbindung zwanzig
Minuten ohne Abriss, ist der Takt bewiesen — das ist im August schon
einmal gemessen worden (22 Minuten), eine Wiederholung ist also nur
eine Bestaetigung.

**Warum das zuerst kommt:** wenn das Messgeraet an einer Stelle anders
ist als das Geraet, will ich das wissen, bevor ich darauf aufbaue.

---

## 2. Die Frequenz-Kodierung endlich beweisen (10 Minuten)

`setReceiverFrequency()` schickt heute eine **vermutete** Kodierung:
Wert mal zehn, acht Bytes, niederwertiges zuerst. Abgeleitet aus
ArtemisSDRs Quelle, nie am Geraet nachgemessen. Das Werkzeug dafuer
liegt seit dem 27.08. bereit:

```bash
sudo python3 tools/sunsdr_freq_confirm.py 20
```

Waehrend es laeuft: in **ExpertSDR2** (nicht in Longpath) auf eine
genau notierte Frequenz abstimmen, z. B. 14 074 000 Hz. Das Werkzeug
druckt jeden 0x08-Rahmen mit der Kandidaten-Formel dekodiert.

* Stimmt die Zahl aufs Hertz → Kodierung bewiesen, der Kandidatenname
  kann aus dem Code verschwinden.
* Ist sie um einen festen Faktor daneben → der Faktor gehoert zur QRP
  (DX und PRO haben zehn).
* Ist es Rauschen → die Annahme faellt, und wir wissen es endlich.

**Gegenprobe ohne ExpertSDR2:** in Longpath abstimmen und schauen, ob
sich das Bild bewegt. Ohne Antenne sieht man wenig, aber die
Eigenstoerungen des Geraets (Traeger vom Takt) wandern mit — das reicht
als Ja/Nein.

---

## 3. Die acht unbekannten Opcodes zuordnen (20–30 Minuten)

Acht Opcodes sind bis heute unzugeordnet: `0x03, 0x0c, 0x0d, 0x0f,
0x11, 0x13, 0x16, 0x1c`. Die meisten gehoeren vermutlich zu
Empfangsdingen — Vorverstaerker, Daempfung, Filter, Abtastrate, AGC —,
und die lassen sich alle ohne Antenne bedienen.

```bash
sudo python3 tools/sunsdr_opcode_watch.py --iface en0 --mark
```

Dann **ExpertSDR2** bedienen und nach jedem Handgriff eine Marke
setzen: Text eintippen, Eingabetaste. Das Protokoll sieht danach so aus:

```
[12:03:15] MARKE: Vorverstaerker ein
12:03:17.412  op=0x0d  len=4  payload=01000000  ?? nicht zugeordnet
[12:03:22] MARKE: Vorverstaerker aus
12:03:23.008  op=0x0d  len=4  payload=00000000  ?? nicht zugeordnet
```

Vorschlag fuer die Reihenfolge — je Schritt eine Marke:

1. ExpertSDR2 starten, verbinden (das ist der Bootvorgang, viele Rahmen)
2. Vorverstaerker ein / aus
3. Daempfung 0 → 10 → 20 dB
4. Abtastrate umstellen (jede Stufe, die das Programm anbietet)
5. Filterbreite aendern
6. AGC umschalten (langsam / schnell / aus)
7. Zweiten Empfaenger ein / aus, falls vorhanden
8. Trennen / ausschalten

Die Ausgabe einfach in eine Datei umleiten und mir geben — die
Zuordnung mache ich dann.

---

## Was ausdruecklich NICHT geht

Alles, was tastet: MOX, Ansteuerung, PA, Antennenwahl beim Senden. Die
vier Kodierer dafuer (0x06, 0x15, 0x17, 0x24) stehen im Code, gehen
aber an keinen Draht, und das bleibt so, bis an einer Antenne oder
einem Abschlusswiderstand gemessen werden kann.
