# Zwei Minuten Mitschnitt — die Anleitung

> **Stand 2026-10-04 — zwei der drei ursprünglichen Fragen sind
> beantwortet, ohne Mitschnitt.** Die Liste unten ist nachgezogen; wer
> den Durchgang macht, soll nicht mehr nach Beantwortetem suchen.
>
> - ~~Welcher Rahmen schaltet den zweiten Empfänger ein?~~ **Erledigt.**
>   Es ist derselbe Rahmen `0x01`: sein erstes Byte trägt die Zahl der
>   Ströme. Zwei Empfänger laufen seit `38988a85` von Ende zu Ende,
>   am Gerät belegt (480 statt 240 Nummern/s, 0 verworfen).
> - ~~Welcher Rahmen stellt die Abtastrate?~~ **Erledigt.** Ebenfalls
>   `0x01`, zweites Byte. Drei Nutzlasten sind durchgemessen.

**Wofür noch:** zwei Fragen, an denen die QRP-Arbeit wirklich noch steht:

1. **Welche Rahmen gehören zum Verbindungsablauf?** Die dreizehn
   bekannten sind unvollständig; gemessen wurden „rund zwei Dutzend".
   Das ist die Grundlage für alles Sendeseitige — die Opcode-Nummern der
   QRP sind nachweislich **andere** als die der DX/PRO, aus der alle
   unbestätigten Zahlen stammen.
2. **Wiederholt ExpertSDR2 bei 96 kHz auch?** Longpath bekommt dort
   rund 110 bytegleiche Wiederholungen je Sekunde (1,2 Kopien je
   Nummer statt 1,0 bei 48 kHz). Drei Gegenmaßnahmen sind gemessen und
   **wirkungslos** (Kopf der Blockantwort spiegeln, zwei Stille-Ströme,
   Quittung vor dem Verwerfen). Zeigt der Mitschnitt dieselben
   Wiederholungen bei ExpertSDR2, ist es die Eigenart des Geräts und
   kein Mangel von Longpath — und die Frage ist erledigt statt offen.

   **Dafür müssen BEIDE Empfänger auf 96 kHz stehen**, nicht nur einer
   und nicht die Vorgabe. Am 2026-10-05 nachgezählt: in den Mitschnitten
   A und B läuft **Kanal 0 mit 48 kHz und Kanal 1 mit 96 kHz** — das
   Gerät kann gemischte Raten, und in dieser Betriebsart liegt die Last
   bei 720 Blöcken/s statt 960. Damit beantworten diese Mitschnitte die
   Frage nicht. Das Werkzeug prüft das jetzt selbst und verweigert die
   Aussage, wenn nicht jeder Kanal auf 96 kHz steht.

**Was dabei nicht gebraucht wird:** keine Antenne, kein Senden, keine
Freigabe für HF. Es wird nur zugehört.

## Vorbereitung

Longpath beenden, damit es dem anderen Programm nicht im Weg steht. Die
QRP hängt an `en9` (eigenes Kabel, 192.168.16.200) — die Schnittstelle
steht im Befehl unten, bei einem anderen Netzweg entsprechend ändern.

## Durchgang A: Verbinden

```bash
sudo tcpdump -i en9 -s 0 -w ~/Desktop/expert-A.pcap host 192.168.16.200
```

Läuft. Jetzt **ExpertSDR2 starten und verbinden lassen**, zehn Sekunden
warten, ExpertSDR2 beenden. Dann `tcpdump` mit **Strg-C** beenden.

## Durchgang B: dasselbe, aber mit zweitem Empfänger

```bash
sudo tcpdump -i en9 -s 0 -w ~/Desktop/expert-B.pcap host 192.168.16.200
```

ExpertSDR2 starten, verbinden, **RX2 einschalten**, zehn Sekunden warten,
beenden. `tcpdump` mit Strg-C beenden.

## Durchgang C (wenn es schnell gehen soll, auch später): Abtastrate

```bash
sudo tcpdump -i en9 -s 0 -w ~/Desktop/expert-C.pcap host 192.168.16.200
```

ExpertSDR2 starten, verbinden, dann **die Abtastrate umstellen** (eine
andere Bandbreite wählen), zehn Sekunden warten, beenden.

## Durchgang D: beide Empfänger auf 96 kHz — der für die Wiederholungsfrage

```bash
sudo tcpdump -i en9 -s 0 -w ~/Desktop/expert-96k.pcap host 192.168.16.200
```

ExpertSDR2 starten, verbinden, **RX2 einschalten und beide Empfänger auf
96 kHz stellen**, zwanzig Sekunden zuhören, beenden. `tcpdump` mit
Strg-C beenden.

Zur Kontrolle, dass es wirklich die richtige Betriebsart war: die
Auswertung muss für **jeden** Kanal 96 kHz melden, also je 480 einzelne
Blöcke je Sekunde. Steht bei einem Kanal 48 kHz, war es wieder die
gemischte Betriebsart.

## Auswerten — ein Befehl je Frage

Der Ablauf im Überblick, und welche Rahmen Longpath nie schickt:

```bash
python3 ~/Longpath/NereusSDR/tools/sunsdr_handshake_diff.py ~/Desktop/expert-A.pcap
```

**Der RX2-Einschalter** — der Rahmen, der sich zwischen A und B
unterscheidet, ist er:

```bash
python3 ~/Longpath/NereusSDR/tools/sunsdr_handshake_diff.py ~/Desktop/expert-A.pcap --vergleich ~/Desktop/expert-B.pcap
```

**Die Abtastrate** — dasselbe mit C:

```bash
python3 ~/Longpath/NereusSDR/tools/sunsdr_handshake_diff.py ~/Desktop/expert-A.pcap --vergleich ~/Desktop/expert-C.pcap
```

Beide Befehle geben am Ende fertige `LONGPATH_SUNSDR_PRE`/`_EXTRA`-Zeilen
aus, mit denen sich derselbe Ablauf **ohne Neubau** ausprobieren lässt.

**Die Wiederholungen** — Frage 2, mit Durchgang D:

```bash
python3 ~/Longpath/NereusSDR/tools/sunsdr_handshake_diff.py --wiederholungen ~/Desktop/expert-96k.pcap
```

Dieser Befehl nennt zuerst die Abtastrate **je Kanal** und sagt dann
selbst, ob der Mitschnitt die Frage beantworten kann. Steht nicht bei
jedem Kanal 96 kHz, verweigert er die Aussage — absichtlich: am
2026-10-04 habe ich aus einem Mitschnitt in der falschen Betriebsart den
falschen Schluss gezogen, und am 2026-10-05 wäre es mit den gemischten
Raten fast wieder passiert.

**Den Rahmen für die gemischten Raten** findet derselbe Vergleich, mit
dem `0x01` als Ratenrahmen gefunden wurde — A (gemischt) gegen D (beide
96 kHz):

```bash
python3 ~/Longpath/NereusSDR/tools/sunsdr_handshake_diff.py ~/Desktop/expert-A.pcap --vergleich ~/Desktop/expert-96k.pcap
```

## Warum der Weg über den Vergleich geht und nicht über Probieren

Die Opcode-Nummern der QRP sind nicht die der DX: bei drei am Gerät
gemessenen Befehlen liegt die QRP um eins darunter, bei `0x01` nicht
(`docs/architecture/2026-10-02-sunsdr-paritaet.md`, Abschnitt 3a). Einen
Opcode zu erraten und ans Funkgerät zu schicken ist deshalb keine
Abkürzung, sondern ein Risiko — zwei Nummern neben `RX2_ENABLE` liegen
Drive und PA-Freigabe. Der Vergleich zweier Mitschnitte beantwortet
dieselbe Frage, ohne etwas zu schicken.

## Prüfen, dass die Auswertung funktioniert, ohne Mitschnitt

```bash
python3 ~/Longpath/NereusSDR/tools/sunsdr_handshake_diff.py --selftest
```

Baut sich ein eigenes pcap und wertet es aus.
